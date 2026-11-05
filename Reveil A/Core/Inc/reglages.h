#ifndef REGLAGES_H
#define REGLAGES_H

#include <stdint.h>
#include "main.h"

/* Commandes de réglage (UART et Bluetooth) -> Config (tâche Gestion_commandes de la conception).
 * Renvoie 1 si la ligne était une commande de réglage (la réponse OK / REFUS est déjà envoyée),
 * 0 sinon (ce n'est pas une commande de réglage). */
uint8_t Reglages_Traiter(UART_HandleTypeDef *uart, const char *ligne, uint8_t len);

/* Affiche la liste des commandes de réglage (à appeler après le menu de Youssef, optionnel) */
void Reglages_Aide(UART_HandleTypeDef *uart);

#endif /* REGLAGES_H */
