/* reglages.c : commandes de réglage reçues par UART ou Bluetooth -> Config (partage.h)
 *   Chaque réglage est vérifié : réponse "OK: ..." si accepté, "REFUS: motif" sinon (conception :
 *   la valeur est gardée seulement si elle est dans la plage autorisée).
 *
 *   M n        mélodie de l'alarme (0 ou 1)
 *   C xx[.x]   consigne de température, de 10 à 35 °C (ex : C 26 ou C 25.5)
 *   Q xxx      seuil de qualité d'air, de 50 à 400 (indice VOC, 100 = air habituel)
 *   D a c      durées de l'aube et du crépuscule en secondes, de 5 à 600 (ex : D 20 30)
 *   L n        aube et crépuscule automatiques : 1 = activés, 0 = désactivés
 *   V          voir tous les réglages
 *
 *   Pas de printf ici : les tâches du terminal ont une petite pile, on construit les textes à la main.
 *   Les lettres p, r, s, x sont déjà prises par les commandes immédiates de Youssef.
 */
#include "reglages.h"
#include "partage.h"
#include <string.h>

/* ---------- Petits outils de texte ---------- */
static void envoyer(UART_HandleTypeDef *uart, const char *texte)
{
  HAL_UART_Transmit(uart, (uint8_t *)texte, (uint16_t)strlen(texte), 100);
}

/* Ajoute un texte à la fin de buf (taille max n) */
static void ajouter(char *buf, uint16_t n, const char *texte)
{
  uint16_t l = strlen(buf);
  while (*texte && l < n - 1)
  {
    buf[l++] = *texte++;
  }
  buf[l] = '\0';
}

/* Ajoute un nombre entier positif à la fin de buf */
static void ajouter_nombre(char *buf, uint16_t n, uint32_t v)
{
  char tmp[11];
  int i = 10;
  tmp[i] = '\0';
  do
  {
    tmp[--i] = '0' + (v % 10);
    v /= 10;
  } while (v > 0 && i > 0);
  ajouter(buf, n, &tmp[i]);
}

/* Ajoute une température avec 1 décimale (ex : 25.5) */
static void ajouter_temperature(char *buf, uint16_t n, float t)
{
  uint32_t dixiemes = (uint32_t)(t * 10.0f + 0.5f);
  ajouter_nombre(buf, n, dixiemes / 10);
  ajouter(buf, n, ".");
  ajouter_nombre(buf, n, dixiemes % 10);
}

/* Lit un entier à partir de *p (avance *p). Renvoie 1 si au moins un chiffre a été lu. */
static uint8_t lire_entier(const char **p, const char *fin, uint32_t *v)
{
  uint8_t chiffres = 0;
  *v = 0;
  while (*p < fin && **p >= '0' && **p <= '9' && chiffres < 6)
  {
    *v = *v * 10 + (uint32_t)(**p - '0');
    (*p)++;
    chiffres++;
  }
  return chiffres > 0;
}

/* Saute les espaces */
static void sauter_espaces(const char **p, const char *fin)
{
  while (*p < fin && **p == ' ')
  {
    (*p)++;
  }
}


/* ---------- Commandes ---------- */
void Reglages_Aide(UART_HandleTypeDef *uart)
{
  envoyer(uart,
    "--- Reglages ---\r\n"
    "M n = MELODIE (0/1)\r\n"
    "C xx = CONSIGNE TEMP (10-35)\r\n"
    "Q xxx = SEUIL AIR (50-400)\r\n"
    "D a c = DUREES AUBE/CREP (s)\r\n"
    "L 1 / L 0 = AUBE/CREP AUTO\r\n"
    "V = VOIR REGLAGES\r\n");
}

static void afficher_reglages(UART_HandleTypeDef *uart)
{
  Config_t cfg;
  char rep[120] = "";

  Config_Lire(&cfg);
  ajouter(rep, sizeof(rep), "Melodie ");
  ajouter_nombre(rep, sizeof(rep), cfg.id_melodie);
  ajouter(rep, sizeof(rep), " | Consigne ");
  ajouter_temperature(rep, sizeof(rep), cfg.Tcons);
  ajouter(rep, sizeof(rep), "C | Seuil air ");
  ajouter_nombre(rep, sizeof(rep), cfg.seuil_IQA);
  ajouter(rep, sizeof(rep), "\r\nAube ");
  ajouter_nombre(rep, sizeof(rep), cfg.D_aube_ms / 1000);
  ajouter(rep, sizeof(rep), "s | Crepuscule ");
  ajouter_nombre(rep, sizeof(rep), cfg.D_crepuscule_ms / 1000);
  ajouter(rep, sizeof(rep), "s | Auto ");
  ajouter(rep, sizeof(rep), (cfg.aube_active && cfg.crepuscule_actif) ? "1" : "0");
  ajouter(rep, sizeof(rep), "\r\n");
  envoyer(uart, rep);
}

uint8_t Reglages_Traiter(UART_HandleTypeDef *uart, const char *ligne, uint8_t len)
{
  const char *p = ligne + 1;
  const char *fin = ligne + len;
  char lettre;
  uint32_t a = 0, b = 0;
  Config_t cfg;
  char rep[64] = "";

  if (len == 0)
  {
    return 0;
  }
  lettre = ligne[0];
  if (lettre >= 'a' && lettre <= 'z')
  {
    lettre = lettre - 'a' + 'A';                  /* majuscule ou minuscule : pareil */
  }
  sauter_espaces(&p, fin);
  Config_Lire(&cfg);

  switch (lettre)
  {
    case 'V':
      if (p != fin) return 0;
      afficher_reglages(uart);
      return 1;

    case 'M':                                      /* M n : mélodie */
      if (!lire_entier(&p, fin, &a) || p != fin) return 0;
      if (a > 1)
      {
        envoyer(uart, "REFUS: melodie 0 ou 1\r\n");
        return 1;
      }
      cfg.id_melodie = (uint8_t)a;
      Config_Ecrire(&cfg);
      ajouter(rep, sizeof(rep), "OK: melodie ");
      ajouter_nombre(rep, sizeof(rep), a);
      break;

    case 'C':                                      /* C xx ou C xx.x : consigne de température */
    {
      uint32_t dixieme = 0;
      if (!lire_entier(&p, fin, &a)) return 0;
      if (p < fin && *p == '.')                    /* une décimale facultative */
      {
        p++;
        if (p < fin && *p >= '0' && *p <= '9')
        {
          dixieme = (uint32_t)(*p - '0');
          p++;
        }
        else return 0;
      }
      if (p != fin) return 0;
      if (a < 10 || a > 35 || (a == 35 && dixieme > 0))
      {
        envoyer(uart, "REFUS: consigne entre 10 et 35 C\r\n");
        return 1;
      }
      cfg.Tcons = (float)a + (float)dixieme / 10.0f;
      Config_Ecrire(&cfg);
      ajouter(rep, sizeof(rep), "OK: consigne ");
      ajouter_temperature(rep, sizeof(rep), cfg.Tcons);
      ajouter(rep, sizeof(rep), " C");
      break;
    }

    case 'Q':                                      /* Q xxx : seuil de qualité d'air */
      if (!lire_entier(&p, fin, &a) || p != fin) return 0;
      if (a < 50 || a > 400)
      {
        envoyer(uart, "REFUS: seuil air entre 50 et 400\r\n");
        return 1;
      }
      cfg.seuil_IQA = (uint16_t)a;
      Config_Ecrire(&cfg);
      ajouter(rep, sizeof(rep), "OK: seuil air ");
      ajouter_nombre(rep, sizeof(rep), a);
      break;

    case 'D':                                      /* D a c : durées aube et crépuscule (s) */
      if (!lire_entier(&p, fin, &a)) return 0;
      sauter_espaces(&p, fin);
      if (!lire_entier(&p, fin, &b) || p != fin) return 0;
      if (a < 5 || a > 600 || b < 5 || b > 600)
      {
        envoyer(uart, "REFUS: durees entre 5 et 600 s\r\n");
        return 1;
      }
      cfg.D_aube_ms = a * 1000;
      cfg.D_crepuscule_ms = b * 1000;
      Config_Ecrire(&cfg);
      ajouter(rep, sizeof(rep), "OK: aube ");
      ajouter_nombre(rep, sizeof(rep), a);
      ajouter(rep, sizeof(rep), " s, crepuscule ");
      ajouter_nombre(rep, sizeof(rep), b);
      ajouter(rep, sizeof(rep), " s");
      break;

    case 'L':                                      /* L 0/1 : aube et crépuscule automatiques */
      if (!lire_entier(&p, fin, &a) || p != fin) return 0;
      if (a > 1)
      {
        envoyer(uart, "REFUS: L 0 ou L 1\r\n");
        return 1;
      }
      cfg.aube_active = (uint8_t)a;
      cfg.crepuscule_actif = (uint8_t)a;
      Config_Ecrire(&cfg);
      ajouter(rep, sizeof(rep), a ? "OK: aube/crepuscule auto actives" : "OK: aube/crepuscule auto desactives");
      break;

    default:
      return 0;                                    /* pas une commande de réglage */
  }

  ajouter(rep, sizeof(rep), "\r\n");
  envoyer(uart, rep);
  return 1;
}
