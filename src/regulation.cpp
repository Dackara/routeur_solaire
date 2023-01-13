/***************************************/
/******** pilotage et regulation   ********/
/***************************************/

#include "regulation.h"
#include "mesure.h"
#include "settings.h"
#include <Arduino.h>

#define variation_lente         1     // config 5
#define variation_normale       10    // config 10
#define variation_rapide        20    // config 20
#define bridePuissance          900   // sur 1000
#define coeff_variation_lente   1
#define coeff_variation_normale 1
#define coeff_variation_rapide  2

#define nbineg                        5
#ifndef utilisation_seuil_intensiteBatterie_bas
#define seuil_intensiteBatterie_bas   0
#endif
#ifndef utilisation_seuil_intensiteBatterie_moyen
#define seuil_intensiteBatterie_moyen 2
#endif


#define coefficient_puisGradmax       0.8
#define coefficient_calPuis           0.5

#define ineg_max                      1000
#define offset_seuilDemarrageBatterie_high 0.2
#define offset_seuilDemarrageBatterie_low -0.5

#define tabmin_size                    10
#define coeff_seuil_deltaxmin          10
#define coeff_marcheForceePercentage   9
#define offset_tempdepart              60000     // 60000ms = 60s
#define coeff_puissanceGradateur_step0 0
#define coeff_puissanceGradateur_step1 350
#define coeff_puissanceGradateur_step2 450
#define coeff_puissanceGradateur_step3 550
#define coeff_puissanceGradateur_step4 650
#define coeff_puissanceGradateur_step5 900

float xmax      = 0;
float xmin      = 0;
int devlente    = 0;
int devdecro    = 0;
float tabxmin[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int itab        = 0;

int RARegulationClass::mesureDerive(float x, float seuil)
{
  int dev  = 0;
  devlente = 0;
  if (x > xmax)
  {
    dev  = 1;
    xmax = x;
    xmin = xmax - seuil;

  } // si dépasse le seuil haut déplacement du seuil haut et bas

  if (x < xmin)
  {
    dev  = -1;
    xmin = x;
    xmax = xmin + seuil;
    for (int i = 0 ; i < (tabmin_size - 1); i++)
    {
      tabxmin[(tabmin_size - 1) - i] = tabxmin[(tabmin_size - 2) - i];
    }
    tabxmin[0] = xmin; // range les valeurs mini
    float deltaxmin = 0;
    // float xminmoy = tabxmin[0];

    for (int i = 1; i < tabmin_size; i++)
    {
      deltaxmin += tabxmin[i] - tabxmin[0];
    } // calcule la pente négative

    if (itab < tabmin_size)
    {
      itab++; // retire les 10 premiers points avant de calculer
    }
    if ((itab >= tabmin_size) && (deltaxmin > coeff_seuil_deltaxmin * seuil))
    {
      devlente = 1;
    }
    else
    {
      devlente = 0;
    }
  } // si descend en dessous du seuil bas déplacement du seuil haut et bas
  return dev;
}

int calPuis     = 0;
int calPuisav   = 0;
int ineg        = 0;
int devprevious = 0;
int devcount    = 0;
int puisGradmax = 0;
int tesTension  = 0;
int chargecomp  = 0;
int testpmax    = 0;

int RARegulationClass::regulGrad(int dev)
{
  calPuisav = calPuis;
 
  // descente réguliere lente
  #ifdef utilisation_seuil_intensiteBatterie_bas
  if ((devlente == 1) && (intensiteBatterie > routeur.sib)) 
  #else  
  if ((devlente == 1) && (intensiteBatterie > seuil_intensiteBatterie_bas))
  #endif
  {
#ifdef utilisation_seuil_intensiteBatterie_moyen
    if (intensiteBatterie > routeur.sim) 
#else     
    if (intensiteBatterie > seuil_intensiteBatterie_moyen)
#endif    
    {
      calPuis -= coeff_variation_rapide * variation_rapide; //descente brutale
    }
    else
    {
      calPuis -= coeff_variation_lente * variation_lente; // fin de charge descente lente
    }
  }
  else
    calPuis += coeff_variation_lente * variation_lente; // autorise la montée si la pente n'est pas descendante

  // courant varie mais reste globalement constant permet le décollage
  if (devlente == 0)
  {
#ifdef utilisation_seuil_intensiteBatterie_moyen    
    if (intensiteBatterie >= routeur.sim ) 
#else   
    if (intensiteBatterie >= seuil_intensiteBatterie_moyen )
#endif
    {
      chargecomp = 0;
      calPuis   += coeff_variation_normale * variation_normale;
    }
#ifdef utilisation_seuil_intensiteBatterie_bas && utilisation_seuil_intensiteBatterie_moyen    
    if ( (intensiteBatterie >= routeur.sib) && (intensiteBatterie < routeur.sim) ) //  if ( (intensiteBatterie >= seuil_intensiteBatterie_bas) && (intensiteBatterie < seuil_intensiteBatterie_moyen) )
#elif utilisation_seuil_intensiteBatterie_bas
    if ( (intensiteBatterie >= routeur.sib) && (intensiteBatterie < seuil_intensiteBatterie_moyen) )
#elif utilisation_seuil_intensiteBatterie_moyen 
    if ( (intensiteBatterie >= seuil_intensiteBatterie_bas) && (intensiteBatterie < routeur.sim) )
#else 
    if ( (intensiteBatterie >= seuil_intensiteBatterie_bas) && (intensiteBatterie < seuil_intensiteBatterie_moyen) )
#endif
        
    {
      chargecomp = 1;
      calPuis   += coeff_variation_normale * variation_normale;
    }
#ifdef utilisation_seuil_intensiteBatterie_bas    
    if ((intensiteBatterie < routeur.sib) && (intensiteBatterie >= -routeur.toleranceNegative)) 
#else
    if ((intensiteBatterie < seuil_intensiteBatterie_bas) && (intensiteBatterie >= -routeur.toleranceNegative))
#endif
    {
      chargecomp = 1;
    }
  }
#ifdef utilisation_seuil_intensiteBatterie_bas
  if ((intensiteBatterie < routeur.sib) && (intensiteBatterie >= -routeur.toleranceNegative)) 
#else 
  if ((intensiteBatterie < seuil_intensiteBatterie_bas) && (intensiteBatterie >= -routeur.toleranceNegative)) 
#endif  
  {
    calPuis += coeff_variation_lente*variation_lente;
  }

  // mesure de la puissance maximum pour le remonter en puissance
  if (calPuis < coefficient_puisGradmax * puisGradmax)
  {
    calPuis += coeff_variation_normale * variation_normale;
  }
  puisGradmax = calPuis;

  // courant devient négatif
  if (intensiteBatterie < -routeur.toleranceNegative)
  {
    if (chargecomp == 0)
      calPuis = coefficient_calPuis * calPuis;
    else
      calPuis -= coeff_variation_normale * variation_normale;
#ifdef utilisation_seuil_intensiteBatterie_bas      
    if ((ineg < ineg_max) && (intensiteBatterie < routeur.sib))
#else     
    if ((ineg < ineg_max) && (intensiteBatterie < seuil_intensiteBatterie_bas))
#endif    
    {
      ineg++;
      //puisGradmax = calPuis-2*variation_rapide;
      if (intensiteBatterie < -routeur.toleranceNegative)
        calPuis -= coeff_variation_normale * variation_normale;
    }             //else while(calPuis>0) { calPuis--; delay(10); }
    if (ineg > nbineg) // autorisation de nbineg mesures négatives avec baisse régulière
    {
      puisGradmax = coefficient_puisGradmax * puisGradmax;
      calPuis    -= coeff_variation_rapide * variation_rapide;
    } // tolerance pic négatif
  }
  else
  {
    ineg = 0;
  }

  devprevious = dev;

  // seuil de démarrage
  if (capteurTension > routeur.seuilDemarrageBatterie + offset_seuilDemarrageBatterie_high)
    tesTension = 1;
  // seuil bas de batterie
  if (capteurTension < routeur.seuilDemarrageBatterie + offset_seuilDemarrageBatterie_low)
    tesTension = 0;
  if (tesTension == 0)
    calPuis = 0;

  if(sortieActive== 0){
#ifdef utilisation_bridesortie1    
    calPuis = min(max(0, calPuis), routeur.bridesortie1); 
 #else
    calPuis = min(max(0, calPuis), bridePuissance);
 #endif   
  }
  else {
#ifdef utilisation_bridesortie2    
    calPuis = min(max(0, calPuis), routeur.bridesortie2);
#else
    calPuis = min(max(0, calPuis), bridePuissance);
 #endif   
  }
  //calPuis = min(max(0, calPuis), bridePuissance);
  return (calPuis);
}

/**************************************/
/******** Pilotage exterieur*******/
/**************************************/

unsigned long tempdepart;
int tempo  = 0;
int tempo2 = 0;
extern int calPuis;

void RARegulationClass::pilotage()
{
  // pilotage du 2eme triac

#ifdef Sortie2
  if (routeur.utilisation2Sorties)
  {
    if ((temperatureEauChaude >= routeur.temperatureBasculementSortie2) && (choixSortie == 0)) // on teste bien qu'on était sur la première sortie également
    {
      choixSortie  = 1;   // on bascule sur la 2ieme sortie
      sortieActive = 2;   // et mets à jour le numéro de la sortie active
    }
    // commande du gradateur2
    if ((temperatureEauChaude < routeur.temperatureRetourSortie1) && (choixSortie == 1) && (tempo2 == 0))
    {
      choixSortie  = 0;
      sortieActive = 1;
    }
    // commande du gradateur1
  }
  else
  {
    choixSortie    = 0;
    sortieActive   = 1;
  }
#endif

  if (routeur.relaisStatique && strcmp(routeur.tensionOuTemperature, "D") == 0)
  {
    if (temperatureEauChaude > routeur.seuilMarche)
    {
      digitalWrite(pinRelais, HIGH); // mise à un du relais statique
      etatRelaisStatique = true;
    }
    if (temperatureEauChaude < routeur.seuilArret)
    {
      digitalWrite(pinRelais, LOW); // mise à zéro du relais statique
      etatRelaisStatique = false;
    }
  }

#ifdef Sortie2
#ifdef Pzem04t

  if ((routeur.utilisation2Sorties) && (!marcheForcee))
  {
    if (choixSortie == 0)
    {
      if ((puissanceDeChauffe < 5) && (puissanceGradateur > 100))
        tempo2++;
      else
        tempo2 = 0; // demarre la tempo chauffe-eau temp atteinte
    }

    if (tempo2 > 10)
    {
      choixSortie  = 1;
      sortieActive = 2;
      tempo2++;
    } // après 2s avec i=0 bascule sur triac2
    if (tempo2 > 200)
    {
      choixSortie  = 0;
      sortieActive = 1;
      tempo2       = 0;
      calPuis      = 0;
    } // après qques minutes bascule sur 1er triac
  }
#endif
#endif

  if (routeur.relaisStatique && strcmp(routeur.tensionOuTemperature, "V") == 0)
  {
    if (capteurTension < routeur.seuilMarche)
    {
      digitalWrite(pinRelais, HIGH); // mise à un du relais statique
      etatRelaisStatique = true;
    }
    if (capteurTension > routeur.seuilArret)
    {
      digitalWrite(pinRelais, LOW); // mise à zéro du relais statique
      etatRelaisStatique = false;
    }
  }
  
  if ((marcheForcee) && (tempo == 0))
  {
    tempdepart = millis(); //  memorisation de moment de depart
    tempo      = 1;
  }
  
  if ((marcheForcee) && (tempo == 1))
  {
    if (marcheForceePercentage == 0)
    {
      puissanceGradateur = coeff_puissanceGradateur_step0;
    }
    else if (marcheForceePercentage == 20)
    {
      puissanceGradateur = coeff_puissanceGradateur_step1;
    }
    else if (marcheForceePercentage == 40)
    {
      puissanceGradateur = coeff_puissanceGradateur_step2;
    }
    else if (marcheForceePercentage == 60)
    {
      puissanceGradateur = coeff_puissanceGradateur_step3;
    }
    else if (marcheForceePercentage == 80)
    {
      puissanceGradateur = coeff_puissanceGradateur_step4;
    }
    else if (marcheForceePercentage == 100)
    {
      puissanceGradateur = coeff_puissanceGradateur_step5;
    }
    else
    {
      puissanceGradateur = coeff_marcheForceePercentage * marcheForceePercentage ;
    }

    calPuis = puissanceGradateur;
    if (millis() > tempdepart + offset_tempdepart )
    { // decremente toutes les minutes
      tempdepart = millis();
      temporisation--;
      paramchange = 1;
    }                       // durée de forcage en milliseconde
    if (temporisation == 0) // fin de temporisation
    {
      marcheForcee = false;
      tempo = 0;
    }
  }
}

void RARegulationClass::desactivation()
{
  digitalWrite(pinTriac, LOW);   // mise à zéro du triac
  digitalWrite(pinSortie2, LOW); // mise à zéro du triac de la sortie 2
  digitalWrite(pinRelais, LOW);  // mise à zéro du relais statique
  puissanceGradateur = 0;
  temporisation      = 0;
  marcheForcee       = false;
}

RARegulationClass RARegulation;
