/**
  ******************************************************************************************************************
  * @file    globales.h 
  * @author  R.NOHE     IUT Informatique La Rochelle
  * @version v1.1
  * @date    11 mai 2017
  * @brief   déclarations des variables globales
  ******************************************************************************************************************/


#ifndef __GLOBALES_H
#define __GLOBALES_H

volatile int lAbscisse = 40;
volatile int lOrdonnee= 120 - (LARGEUR_VOITURE - HAUTEUR_VOITURE)/2.f;;

/*struct ChangementCoor{
	int changementAbscisse;
	int changementOrdonnee;
};*/

struct Coordonnee{
	int abscisse;
	int ordonnee;
};

struct Compteur{
	int wakeUp;
	int tamper;
	int joyStickUp;
	int joyStickDown;
};

struct Mur{
	struct Coordonnee origine;
	bool horizontal;
	int longueur;
};

enum Orientation {UP, RIGHT, DOWN, LEFT};
	

#endif
