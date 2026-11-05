/* SGP40.c : capteur de qualité d'air SGP40 (librairie du cours, corrigée pour ce projet)
 *
 * Mêmes fonctions que la version campus (SGP40_Init, SGP40_MeasureRaw, SGP40_MeasureVOC).
 * Corrections :
 *  1. Ce projet n'a pas de fichier "i2c.h" : on déclare hi2c1 directement.
 *  2. BUG : la version campus lisait la mesure dans un uint16_t, ce qui inverse les 2 octets
 *     (le STM32 range l'octet de poids faible en premier). L'algorithme VOC recevait donc
 *     une valeur fausse. Ici on lit octet par octet, dans le bon ordre.
 *  3. Le CRC de chaque réponse est vérifié : une mesure abîmée est ignorée.
 *  4. osDelay au lieu de HAL_Delay : pendant l'attente, la tâche laisse le processeur aux autres.
 *  5. Délai max de 100 ms sur l'I2C au lieu d'une attente infinie (capteur absent = pas de blocage).
 *  6. Pas de printf (pas de console ici, et printf consomme beaucoup de pile).
 *  7. L'algorithme VOC est toujours initialisé, même si l'auto-test échoue.
 *  8. Supprimé : la commande "feature set" (0x202F, commande du SGP30, pas du SGP40)
 *     et le "reset" envoyé à l'adresse 0x59 (c'est une commande d'appel général, pas du SGP40).
 */
#include "SGP40.h"
#include "main.h"
#include "cmsis_os.h"

extern I2C_HandleTypeDef hi2c1;

#define SGP40_I2C       (SGP40_ADDR << 1)
#define TIMEOUT_I2C_MS  100

static const uint8_t CRC_TABLE[256] = {
	0, 49, 98, 83, 196, 245, 166, 151, 185, 136, 219, 234, 125, 76, 31, 46,
	67, 114, 33, 16, 135, 182, 229, 212, 250, 203, 152, 169, 62, 15, 92, 109,
	134, 183, 228, 213, 66, 115, 32, 17, 63, 14, 93, 108, 251, 202, 153, 168,
	197, 244, 167, 150, 1, 48, 99, 82, 124, 77, 30, 47, 184, 137, 218, 235,
	61, 12, 95, 110, 249, 200, 155, 170, 132, 181, 230, 215, 64, 113, 34, 19,
	126, 79, 28, 45, 186, 139, 216, 233, 199, 246, 165, 148, 3, 50, 97, 80,
	187, 138, 217, 232, 127, 78, 29, 44, 2, 51, 96, 81, 198, 247, 164, 149,
	248, 201, 154, 171, 60, 13, 94, 111, 65, 112, 35, 18, 133, 180, 231, 214,
	122, 75, 24, 41, 190, 143, 220, 237, 195, 242, 161, 144, 7, 54, 101, 84,
	57, 8, 91, 106, 253, 204, 159, 174, 128, 177, 226, 211, 68, 117, 38, 23,
	252, 205, 158, 175, 56, 9, 90, 107, 69, 116, 39, 22, 129, 176, 227, 210,
	191, 142, 221, 236, 123, 74, 25, 40, 6, 55, 100, 85, 194, 243, 160, 145,
	71, 118, 37, 20, 131, 178, 225, 208, 254, 207, 156, 173, 58, 11, 88, 105,
	4, 53, 102, 87, 192, 241, 162, 147, 189, 140, 223, 238, 121, 72, 27, 42,
	193, 240, 163, 146, 5, 52, 103, 86, 120, 73, 26, 43, 188, 141, 222, 239,
	130, 179, 224, 209, 70, 119, 36, 21, 59, 10, 89, 104, 255, 206, 157, 172
};

VocAlgorithmParams voc_algorithm_params;

/* CRC des données Sensirion (polynôme 0x31, départ 0xFF) */
static uint8_t crc8(uint8_t msb, uint8_t lsb)
{
	uint8_t crc = 0xFF;
	crc ^= msb;
	crc = CRC_TABLE[crc];
	crc ^= lsb;
	crc = CRC_TABLE[crc];
	return crc;
}

/* Envoie une commande, attend, puis lit la réponse : 2 octets + 1 CRC.
 * Renvoie 1 si tout s'est bien passé. */
static uint8_t commande(uint8_t *cmd, uint16_t n, uint32_t attente_ms, uint16_t *reponse)
{
	uint8_t rx[3];

	if (HAL_I2C_Master_Transmit(&hi2c1, SGP40_I2C, cmd, n, TIMEOUT_I2C_MS) != HAL_OK)
		return 0;
	osDelay(attente_ms);
	if (HAL_I2C_Master_Receive(&hi2c1, SGP40_I2C, rx, 3, TIMEOUT_I2C_MS) != HAL_OK)
		return 0;
	if (crc8(rx[0], rx[1]) != rx[2])
		return 0;

	*reponse = ((uint16_t)rx[0] << 8) | rx[1];   /* octet de poids fort en premier */
	return 1;
}

/* Renvoie 0 si l'auto-test est réussi, 1 sinon */
uint8_t SGP40_Init(void)
{
	uint8_t auto_test[2] = { 0x28, 0x0E };
	uint16_t r = 0;
	uint8_t erreur = 1;

	/* Auto-test : réponse 0xD400 = OK, 0x4B00 = défaillant (320 ms minimum) */
	if (commande(auto_test, 2, 400, &r) && r == 0xD400)
		erreur = 0;

	VocAlgorithm_init(&voc_algorithm_params);
	return erreur;
}

/* Mesure brute (ticks), compensée en température et humidité.
 * Renvoie 0 si la mesure a échoué (l'algorithme VOC ignore alors cette valeur). */
uint16_t SGP40_MeasureRaw(float temp, float humi)
{
	uint8_t cmd[8];
	uint16_t paramh, paramt, sraw = 0;

	if (temp < -45) temp = -45;            /* plage acceptée par le capteur */
	if (temp > 129) temp = 129;
	if (humi < 0)   humi = 0;
	if (humi > 100) humi = 100;

	paramh = (uint16_t)((humi * 65535) / 100 + 0.5f);     /* datasheet page 13 */
	paramt = (uint16_t)(((temp + 45) * 65535) / 175);

	cmd[0] = 0x26;  cmd[1] = 0x0F;                        /* commande "measure raw" */
	cmd[2] = paramh >> 8;  cmd[3] = paramh & 0xFF;  cmd[4] = crc8(cmd[2], cmd[3]);
	cmd[5] = paramt >> 8;  cmd[6] = paramt & 0xFF;  cmd[7] = crc8(cmd[5], cmd[6]);

	if (!commande(cmd, 8, 30, &sraw))
		return 0;
	return sraw;
}

/* Indice VOC : 0 pendant les 45 premières secondes, puis de 1 à 500 (100 = moyenne de la pièce).
 * À appeler une fois par seconde. */
uint16_t SGP40_MeasureVOC(float temp, float humi)
{
	int32_t voc_index;
	uint16_t sraw = SGP40_MeasureRaw(temp, humi);

	VocAlgorithm_process(&voc_algorithm_params, sraw, &voc_index);
	return voc_index;
}
