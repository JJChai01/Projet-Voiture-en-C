/**
  ******************************************************************************************************************
  * @file    main.c 
  * @author  R.NOHE     IUT Informatique La Rochelle
  * @version v1.1
  * @date    11 mai 2017
  * @modification	rien pour le moment
  * @brief   template pour piloter l'afficheur LCD des cartes MCBSTM32EXL
  ******************************************************************************************************************/

#include "stm32f10x.h"                  /* STM32F10x.h definitions            */
#include "GLCD_Config.h"
#include "boule.h"
#include "ext_globales.h"
#include "globales.h"
#include <stdlib.h>
#include <stdio.h>


#define UIE (1<<0)
#define UIF (1<<0)
#define AFIO_EXTICR1 *(volatile unsigned long *)0x40010008
#define AFIO_EXTICR4 *(volatile unsigned long *)0x40010014
#define SETENA0 *(volatile unsigned long *)0xE000E100
#define SETENA1 *(volatile unsigned long *)0xE000E104

int renvoyeAbsolu(int nb);
void cfgTimer1(void);
void cfgGPIO(void);
void dessinerVoiture(void);
void gereChangementDirection(void);
int renvoieValuerARR(int vitessePixel);
int resetCompteur(int vitessePixel);
void dessinerVitesse(void);
void gererBord(void);
void dessinerRectangle(void);
void drawComponent(void);
struct Mur* newMur(int uneAbscisse, int uneOrdonnee, bool bVertical, int uneLongueur);
void creerObstacles(void);
void creerMurs(void);
void storeEtDessineMur(struct Mur *unMur);
void sort(unsigned long a[], int sizeArray); 
bool searchIdPixel(unsigned long a[], int sizeArray, int pixelACherche); 
void gererObstacles(void);
bool estImpossibleDeTourner(void);

int vitessePixel; // vitesse de la voiture 
enum Orientation orientation = RIGHT;

/*
Explication de la variable decaleDeCentre

	1   _	  2  _ _ 
		|  |     l_ _ l
		|_|
		
Le point où la carte dessine la voiture commence à 1 (si elle est verticale) ou 2 (si elle est horizontale)
Pour passer de 1 à 2 (quand la voiture tourne), on faire lAbsicisse - decaleDeCentre, lOrdonnee + decaleDeCentre.
*/
float decaleDeCentre = (LARGEUR_VOITURE - HAUTEUR_VOITURE)/2.f;
bool directionChange = false; //quand la voiture tourne, directionChange = true
struct Compteur compteur; // compteur pour gerer le rebondissement pour chaque 
struct Coordonnee changementCoor; //variable pour determiner l'avancement de la voiture en fonction de sa direction.
bool vitesseChange = true; // chaque fois il y a un changement de vitesse (par exemple si on passe de 2 à 3), vitesseChange = true
unsigned long aPixelIdMur[3000]; // un array pour stocker les positions des obstacles "Mur", pour qu'on puisse les detecte
int indexAMur = 0; // la vraie taille de aPixelIdMur


/*----------------------------------------------------------------------------
  Main Program
 *----------------------------------------------------------------------------*/
int main (void) {
	//initialization des variables
	vitessePixel = 0;
	compteur.wakeUp = 0;
	compteur.tamper = 0;
	compteur.joyStickUp = 0;
	compteur.joyStickDown = 0;
	changementCoor.abscisse = 1;
	changementCoor.ordonnee = 0;
	
	//initialization d'écran
  GLCD_Initialize();                          /* Initialize graphical LCD display   */
  GLCD_SetBackgroundColor(GLCD_COLOR_BLACK);
	GLCD_SetForegroundColor(GLCD_COLOR_WHITE);
	GLCD_ClearScreen();               /* Clear graphical LCD display        */
	GLCD_SetFont(&GLCD_Font_6x8);
	//GLCD_DrawBitmap(lAbscisse,lOrdonnee,
									//LARGEUR_VOITURE, HAUTEUR_VOITURE,
									//(const unsigned char *)bmpyellowcarpngRight);
	GLCD_FrameBufferAccess(true); // à faire avant appel à GLCD_DrawPixel et GLCD_ReadPixel pour supprimer
																// les limitations d'accès à la SRAM (uniquement nécessaire sur les anciennes
																// cartes sinon les fonctions GLCD_DrawPixel et GLCD_ReadPixel ne 
																// fonctionnent pas

	
	//Configuration des ports et des timer
	cfgGPIO();
	cfgTimer1();
	
	//dessiner les components graphiques (comme le String "vitessse", nom...);
	drawComponent();
	
	//creer des obstacles, les stocker (pour qu'on puisse les detecte) et les dessiner. Des obstacles sont les murs, sangliers...
	creerObstacles();
	
	while(1){
		dessinerVoiture();
		//dessiner la vitisse actuelle de la voiture
		if(vitesseChange){
			dessinerVitesse();
			vitesseChange = false;
		}
		//rectangle noire pour effacer la trace de l'ancienne voiture
		dessinerRectangle();
  }
	
}


int renvoyeAbsolu(int nb){
	if(nb < 0)
		nb = -nb;	
	return nb;
}


void cfgTimer1(void) {
	RCC->APB2ENR |= (1<<11);	//Validation timer1
	TIM1->PSC = 57;
	TIM1->ARR = 64000;
	TIM1->DIER |= UIE;
	TIM1->CR1 |= 0x0001;			//Démarrage timer1
	SETENA0 |= (1<<25); //SETENA0 ajout de l'interruption n°25 (cf TD3 page 2)
	
}

void cfgGPIO(void) {
	
	unsigned int temp;
	//---Mise en service des registres AFIO pour la configuration d'interruptions externes---
	RCC->APB2ENR |= (1<<0); //Validation AFIO clock
	
	//---Configuration de JOYSTICK_UP sur PG15---
	RCC->APB2ENR |= (1 << 8); //Validation GPIOG clock
	temp = GPIOG->CRH & 0x0FFFFFFF;
  GPIOG->CRH = temp | 0x40000000; //Activation la broche 15 de GPIOG, notamment pour JOYSTICK... (regarder documentation CRH)
	
	//---Configuration de JOYSTICK_DOWN sur PD3---
	RCC->APB2ENR |= (1 << 5); //Validation GPIOD clock
	temp = GPIOD->CRL & 0xFFFF0FFF;
  GPIOG->CRL = temp | 0x00004000;
	
	//---Configuration de BP_WAKEUP sur PD3---
	RCC->APB2ENR |= (1 << 2); //Validation GPIOA clock
	temp = GPIOA->CRL & 0xFFFFFFF0;
  GPIOA->CRL = temp | 0x00000004;
	
	//---Configuration de BP_TAMPER sur PC13---
	RCC->APB2ENR |= (1 << 4); //Validation GPIOC clock
	temp = GPIOC->CRH & 0xFF0FFFFF;
  GPIOC->CRH = temp | 0x00400000;
	
	//---Configuration des ISR sur EXTI15 (JOYSTIK_UP)---
	SETENA1 |= (1<<8); //SETENA1 ajout de l'interruption n°40 (32+8) pour activation EXTI15_10_IRQHandler
	temp = AFIO_EXTICR4 & 0x0FFF;
	AFIO_EXTICR4 = temp | 0x6000;
	EXTI->IMR |= (1<<15);
	EXTI->FTSR |= (1<<15); //Event sur front descendant
	
	//---Configuration des ISR sur EXTI3 (JOYSTIK_DOWN)---
	SETENA0 |= (1<<9); //SETENA0 ajout de l'interruption n°9 pour activation EXTI3_IRQHandler
	temp = AFIO_EXTICR1 & 0x0FFF;
	AFIO_EXTICR1 = temp | 0x3000;
	EXTI->IMR |= (1<<3);
	EXTI->FTSR |= (1<<3); //Event sur front descendant
	
	//---Configuration des ISR sur EXTI0 (BP_WAKEUP)---
	SETENA0 |= (1<<6); //SETENA0 ajout de l'interruption n°6 pour activation EXTI0_IRQHandler
	AFIO_EXTICR1 &= 0xFFF0;
	EXTI->IMR |= (1<<0);
	EXTI->RTSR |= (1<<0); //Event sur front montant
	
	//---Configuration des ISR sur EXTI13 (BP_TAMPER)---
	temp = AFIO_EXTICR4 & 0xFF0F;
	AFIO_EXTICR4 = temp | 0x0020;
	EXTI->IMR |= (1<<13);
	EXTI->FTSR |= (1<<13); //Event sur front montant
}

void TIM1_UP_TIM10_IRQHandler (void) {	//Timer1 	
	if(TIM1->SR & UIF)
	{
		TIM1->SR &= ~UIF;
		
		//ces compteurs servent à gerer la rebondissement des boutons. Quand on appuye, on reset le compteur. 
		if(compteur.wakeUp>0) compteur.wakeUp--;
		if(compteur.tamper>0) compteur.tamper--;
		if(compteur.joyStickUp>0) compteur.joyStickUp--;
		if(compteur.joyStickDown>0) compteur.joyStickDown--;
		
		//empêcher la voiture sort de "l'écran"
		gererBord();
		
		//quand la voiture touche des obstacles, elle s'arrête
		gererObstacles();
		
		//l'avancement de la voiture
		if(vitessePixel<0){
			lAbscisse -= changementCoor.abscisse;
			lOrdonnee -= changementCoor.ordonnee;
		}
		else if(vitessePixel>0){
			lAbscisse += changementCoor.abscisse;
			lOrdonnee += changementCoor.ordonnee;
		}
	}
}

void EXTI15_10_IRQHandler(void) {
		if(EXTI->PR & (1<<15))
		{
				EXTI->PR |= (1<<15);
				if(compteur.joyStickUp <= 0)
				{
					//gestion de rebondissement
					compteur.joyStickUp = resetCompteur(vitessePixel);
					if(vitessePixel<100 && (GPIOG->IDR & (1<<15)) == 0)
					{
						vitessePixel+=20;		
						vitesseChange = true;			
						TIM1->ARR = renvoieValuerARR(vitessePixel);
					}
				}
		}	
		if(EXTI->PR & (1<<13)){
			EXTI->PR |= (1<<13);
					if(compteur.tamper <= 0) //Tamper
					{
						//gestion de rebondissement
						compteur.tamper = resetCompteur(vitessePixel);
						if((GPIOC->IDR & (1<<13)) == 0 && !estImpossibleDeTourner()){
							orientation = (enum Orientation)((orientation + 4 - 1) % 4); 
							gereChangementDirection();
						}
					}
		}
}
void EXTI0_IRQHandler(void) {
		if(EXTI->PR & (1<<0)) //WakeUp
	  {
			EXTI->PR |= (1<<0);
			if(compteur.wakeUp <= 0){
				//gestion de rebondissement
				compteur.wakeUp = resetCompteur(vitessePixel);
				if((GPIOA->IDR & (1<<0)) == 1 && !estImpossibleDeTourner()){
					orientation = (enum Orientation)((orientation + 1) % 4);
					gereChangementDirection();
				}
			}
		}
}
int resetCompteur(int vitessePixel){
	  int compteurTemp;
		if(vitessePixel == 0)
			compteurTemp = 4; // unNombre * 0.05 = le temps d'attente avant qu'on peut appuyer les bouton pour une deuxieme fois. ça sert à empecher les rebondissements
		else
			compteurTemp = 4*(renvoyeAbsolu(vitessePixel)/20); 	
		return compteurTemp;
}
void EXTI3_IRQHandler(void) {
		if(EXTI->PR & (1<<3))
		{
			EXTI->PR |= (1<<3);
			if(compteur.joyStickDown <= 0) {
				//gestion de rebondissement
				compteur.joyStickDown = resetCompteur(vitessePixel);
				if(((GPIOD->IDR & (1<<3)) == 0)&& vitessePixel>-20){
					vitessePixel-=20;
					vitesseChange = true;
					TIM1->ARR = renvoieValuerARR(vitessePixel);
				}
			}
		}
}

void dessinerVoiture(void) {
	switch(orientation){
		case UP:
			GLCD_DrawBitmap(lAbscisse,lOrdonnee,
									HAUTEUR_VOITURE, LARGEUR_VOITURE,
									(const unsigned char *)bmpyellowcarpngTop);
			break;
		case RIGHT:
			GLCD_DrawBitmap(lAbscisse,lOrdonnee,
									LARGEUR_VOITURE, HAUTEUR_VOITURE,
									(const unsigned char *)bmpyellowcarpngRight);
			break;
		case DOWN:
			GLCD_DrawBitmap(lAbscisse,lOrdonnee,
									HAUTEUR_VOITURE, LARGEUR_VOITURE,
									(const unsigned char *)bmpyellowcarpngDown);
			break;
		case LEFT:
			GLCD_DrawBitmap(lAbscisse,lOrdonnee,
									LARGEUR_VOITURE, HAUTEUR_VOITURE,
									(const unsigned char *)bmpyellowcarpngLeft);
			break;
	}
}


void gereChangementDirection(void){
	directionChange = true;
	switch(orientation){
		case UP:
			changementCoor.abscisse = 0;
			changementCoor.ordonnee = -1;
		  lAbscisse += decaleDeCentre;
		  lOrdonnee -= decaleDeCentre;
			break;
		case RIGHT:
			changementCoor.abscisse = 1;
			changementCoor.ordonnee = 0;		
		  lAbscisse -= decaleDeCentre;
		  lOrdonnee += decaleDeCentre;
			break;
		case DOWN:
			changementCoor.abscisse = 0;
			changementCoor.ordonnee = 1;
		  lAbscisse += decaleDeCentre;
		  lOrdonnee -= decaleDeCentre;	
			break;
		case LEFT:
			changementCoor.abscisse = -1;
			changementCoor.ordonnee = 0;			
		  lAbscisse -= decaleDeCentre;
		  lOrdonnee += decaleDeCentre;	
			break;
	}	
	
}


int renvoieValuerARR(int vitessePixel){
	//quand vitessePixel == 0, on met ARR = 64000. On met la valeur la plus grande possible pour ARR (pour economiser cout d'operation). Parce que on entre TIM1 pour faire bouger la voiture.
	if(vitessePixel == 0)
		return 64000;
	else 
		return 64000/(renvoyeAbsolu(vitessePixel/20));
}

void dessinerVitesse(void){
	GLCD_DrawBargraph(300, 220, 10, 10, 0x0000);	
	if(vitessePixel>=0)
		GLCD_DrawChar(300, 220, '0'+vitessePixel/20);
	else{
		GLCD_DrawChar(300, 220, '-');
		GLCD_DrawChar(305, 220, '0'+(-vitessePixel/20));
	}
}
void gererBord(void){
	//pour que la voiture sort pas de l'écran
	if((lAbscisse == GLCD_SIZE_Y-LARGEUR_VOITURE && changementCoor.abscisse > 0 && vitessePixel > 0) || 
			(lAbscisse == GLCD_SIZE_Y-LARGEUR_VOITURE && changementCoor.abscisse < 0 && vitessePixel < 0) ||
		  (lAbscisse == 0 && changementCoor.abscisse < 0 && vitessePixel > 0) ||
			(lAbscisse == 0 && changementCoor.abscisse > 0 && vitessePixel < 0)
		||
		  (lOrdonnee == GLCD_SIZE_X-LARGEUR_VOITURE && changementCoor.ordonnee > 0 && vitessePixel > 0) ||
		  (lOrdonnee == GLCD_SIZE_X-LARGEUR_VOITURE && changementCoor.ordonnee < 0 && vitessePixel < 0) ||
		  (lOrdonnee == 0 && changementCoor.ordonnee < 0 && vitessePixel > 0) ||
		  (lOrdonnee == 0 && changementCoor.ordonnee > 0 && vitessePixel < 0) 
		){
			vitessePixel = 0;
			vitesseChange = true;
		}		
}

void dessinerRectangle(void){
		//pour effacer "la trace" de la voiture quand elle bouge
		if(changementCoor.abscisse > 0) {
			if(vitessePixel > 0 )
				GLCD_DrawBargraph(lAbscisse-1, lOrdonnee, 1, 12, 0x0000);
			else if(vitessePixel < 0) 
				GLCD_DrawBargraph(lAbscisse+LARGEUR_VOITURE, lOrdonnee, 1, 12, 0x0000);
		} 
		else if(changementCoor.abscisse < 0){
			if(vitessePixel < 0 )
				GLCD_DrawBargraph(lAbscisse-1, lOrdonnee, 1, 12, 0x0000);
			else if(vitessePixel > 0) 
				GLCD_DrawBargraph(lAbscisse+LARGEUR_VOITURE, lOrdonnee, 1, 12, 0x0000);			
		}
	  else if(changementCoor.ordonnee > 0){
			if(vitessePixel > 0 ) 
				GLCD_DrawBargraph(lAbscisse, lOrdonnee-1, 12, 1, 0x0000);
			else if(vitessePixel < 0) 
				GLCD_DrawBargraph(lAbscisse, lOrdonnee+LARGEUR_VOITURE, 12, 1, 0x0000);		  
	  }
		else if(changementCoor.ordonnee < 0){
			if(vitessePixel < 0 ) 
				GLCD_DrawBargraph(lAbscisse, lOrdonnee-1, 12, 1, 0x0000);
			else if(vitessePixel > 0) 
				GLCD_DrawBargraph(lAbscisse, lOrdonnee+LARGEUR_VOITURE, 12, 1, 0x0000);		  	
		}
		
		//pour effacer "la trace" de la voiture quand elle tourne
		if(directionChange){
			if(orientation == 1 || orientation == 3) {// ça veut dire elle vient de tourner de de la position verticale à la position horizontale
				GLCD_DrawBargraph(lAbscisse+decaleDeCentre, lOrdonnee-decaleDeCentre, HAUTEUR_VOITURE, decaleDeCentre, 0x0000);
				GLCD_DrawBargraph(lAbscisse+decaleDeCentre, lOrdonnee+HAUTEUR_VOITURE, HAUTEUR_VOITURE, decaleDeCentre, 0x0000);
				GLCD_DrawBargraph(lAbscisse+decaleDeCentre-1, lOrdonnee-decaleDeCentre, 5, LARGEUR_VOITURE, 0x0000);
				GLCD_DrawBargraph(lAbscisse+decaleDeCentre+HAUTEUR_VOITURE-2, lOrdonnee-decaleDeCentre, 5, LARGEUR_VOITURE, 0x0000);
			}
			else{ // ça veut dire elle vient de tourner de de la position horizontale à la position verticale
				GLCD_DrawBargraph(lAbscisse-decaleDeCentre, lOrdonnee+decaleDeCentre, decaleDeCentre, HAUTEUR_VOITURE, 0x0000);
				GLCD_DrawBargraph(lAbscisse+HAUTEUR_VOITURE, lOrdonnee+decaleDeCentre, decaleDeCentre, HAUTEUR_VOITURE, 0x0000);
				GLCD_DrawBargraph(lAbscisse-decaleDeCentre, lOrdonnee+decaleDeCentre-1, LARGEUR_VOITURE, 5, 0x0000);
				GLCD_DrawBargraph(lAbscisse-decaleDeCentre, lOrdonnee+decaleDeCentre+HAUTEUR_VOITURE-2, LARGEUR_VOITURE, 5, 0x0000);
			}
			directionChange = false;
		}
}

void drawComponent(void){
	GLCD_DrawString(5, 5, "CHAI Jia Jun, PEUZIAT Thomas, D2");
	GLCD_DrawString(240, 220, "Vitesse : ");
}

struct Mur* newMur(int uneAbscisse, int uneOrdonnee, bool bHorizontal, int uneLongueur){
	//constructor pour un struct Mur
	struct Mur *mur = malloc(sizeof(struct Mur));
  mur->origine.abscisse = uneAbscisse;
	mur->origine.ordonnee = uneOrdonnee;
	mur->horizontal = bHorizontal;
	mur->longueur = uneLongueur;
	return mur;
}

void creerObstacles(void){
	//Pour organiser la création des obstacles: Murs, Sangliers....
	creerMurs();
	//creerSangliers();
}

void creerMurs(void){
	struct Mur *murHori1 = newMur(0, 100, true, 20);
	struct Mur *murHori2 = newMur(0, 140, true, 20);
	struct Mur *murVert1 = newMur(20, 20, false, 80);
	struct Mur *murVert2 = newMur(20, 140, false, 70);
	struct Mur *murVert3 = newMur(80, 80, false, 80);
	struct Mur *murHori3 = newMur(20, 20, true, 280);
	struct Mur *murVert4 = newMur(140, 20, false, 80);
	struct Mur *murHori4 = newMur(20, 210, true, 280);
	struct Mur *murVert5 = newMur(300, 20, false, 190);
	struct Mur *murHori5 = newMur(80, 160, true, 160);
	struct Mur *murVert6 = newMur(200, 120, false, 15);
	struct Mur *murVert7 = newMur(230, 100, false, 15);
	struct Mur *murHori6 = newMur(190, 65, true, 30);
	struct Mur *murVert8 = newMur(255, 75, false, 10);
	
	storeEtDessineMur(murHori1);
	storeEtDessineMur(murHori2);
	storeEtDessineMur(murHori3);
	storeEtDessineMur(murHori4);
	storeEtDessineMur(murHori5);
	storeEtDessineMur(murHori6);

	storeEtDessineMur(murVert1);
	storeEtDessineMur(murVert2);
	storeEtDessineMur(murVert3);
	storeEtDessineMur(murVert4);
	storeEtDessineMur(murVert5);
	storeEtDessineMur(murVert6);
	storeEtDessineMur(murVert7);
	storeEtDessineMur(murVert8);

	//On arrange la collection contenant les pixelId de tous les murs ( pour qu'on puisse analyser la collection plus rapidement avec Binary Search)
	//un pixelId est un id unique pour chaque pixel possible sur l'écran
	sort(aPixelIdMur, indexAMur);

}

void storeEtDessineMur(struct Mur *unMur){
	//un pixelId est un id unique pour chaque pixel possible sur l'écran
	unsigned long pixelId;
	
	if(unMur->horizontal){
		int i;
		for(i = 0; i < (unMur->longueur); i++){
			//on calcule une id unique pour chaque pixel (dans ce cas, ce sont les pixels pour les obstacles murs), et on les stocke dans une collection qui s'appelle aPixelIdMur pour qu'on puisse detecter les murs plus tard
			pixelId = (unMur->origine.ordonnee)*320 + unMur->origine.abscisse + i;
			aPixelIdMur[indexAMur++] = pixelId;
		}
		
		GLCD_DrawHLine(unMur->origine.abscisse, unMur->origine.ordonnee, unMur->longueur);
	}
	else{
		int i;
		for(i = 0; i < (unMur->longueur); i++){
			pixelId = (unMur->origine.ordonnee + i)*320 + unMur->origine.abscisse;
			aPixelIdMur[indexAMur++] = pixelId;
		}
		
		GLCD_DrawVLine(unMur->origine.abscisse, unMur->origine.ordonnee, unMur->longueur);
	}	
}

void sort(unsigned long a[], int sizeArray){
	//INSERTION SORT
	int i;
	int newPlaceForKey;
	
	for(i = 1; i < sizeArray; i++){
		unsigned long key = a[i];
		int j = i - 1;
		while(a[j] > key && j >= 0)
			j--;
		j++;
		newPlaceForKey = j;
		j = i - 1;
		while(j >= newPlaceForKey){
			a[j+1] = a[j];
			j--;
		}	
		a[newPlaceForKey] = key;
	}
}


bool searchIdPixel(unsigned long array[], int sizeArray, int idPixelACherche){
	//Binary Search
	int a = 0, b, c = sizeArray;
	bool trouve = false;
	
	while(!trouve){
		b = ((c - a)/2) + a;
		if(array[b] == idPixelACherche)
      trouve = true;
    else if (array[b] < idPixelACherche)
      a = b;
    else if(array[b] > idPixelACherche)
      c = b;
		
    if(c - a == 1)
      break;
	}
	
	if(trouve || array[a] == idPixelACherche) // on verifie array[a] parce qu'on a fait if((c - a) == 1), break;  On verifie pas si array[a] est la solution quand c - a == 1
		trouve = true;
	else
		trouve = false;
	
	return trouve;
}

void gererObstacles(void){
	//une fonction pour detecter si devant la voiture il y a un obstacle
	//un pixelId est une id unique pour chaque pixel possible sur l'écran. Dans cette fonction on va prendre les pixelId devant la voiture, et les comparer avec les pixelId des obstacles (qui sont stockés dans des collections) 
	int pixelIdAVerifier = 0;
	int distanceFromObstacle = 2;
	int i;
	for(i = 0; i < HAUTEUR_VOITURE; i++)
	{
		// UP positive ou DOWN negative
		if((vitessePixel > 0 && orientation == 0) || (vitessePixel < 0 && orientation == 2)){
			pixelIdAVerifier = (lOrdonnee - distanceFromObstacle)*320 + (lAbscisse + i);
		} 
		
		// UP negative ou DOWN positive
		else if ((vitessePixel < 0 && orientation == 0) || (vitessePixel > 0 && orientation == 2)){
			pixelIdAVerifier = (lOrdonnee + LARGEUR_VOITURE + distanceFromObstacle)*320 + (lAbscisse + i);
		} 
		
		// RIGHT positive ou LEFT negative
		else if ((vitessePixel > 0 && orientation == 1) || (vitessePixel < 0 && orientation == 3)){
			pixelIdAVerifier = (lOrdonnee + i)*320 + (lAbscisse + LARGEUR_VOITURE + distanceFromObstacle);
		} 
		
		// RIGHT negative ou LEFT positive
		else if ((vitessePixel < 0 && orientation == 1) || (vitessePixel > 0 && orientation == 3)){	
			pixelIdAVerifier = (lOrdonnee + i)*320 + (lAbscisse - distanceFromObstacle);
		}

		//si on trouve un mur avec le meme pixelId (ca veut dire la voiture 'touche' le mur), la voiture elle s'arrete.
		if(searchIdPixel(aPixelIdMur, indexAMur, pixelIdAVerifier)){
			vitessePixel = 0;
			vitesseChange = true;
			break;
		}
	}
}
	
bool estImpossibleDeTourner(void){
	//dans cette fonction on analyse si la voiture est possible de tourner. 
	bool impossibleDeTourner = false;
	int i;
	
	// ça va dire elle veut tourner de vertical à position horizontal
	if(orientation == 0 || orientation == 2){ 
		
		//les deux for loop detecte la front de la voiture
		for(i = 0; !impossibleDeTourner && i < decaleDeCentre; i++){
			impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse - decaleDeCentre + i + (lOrdonnee + decaleDeCentre)*320);
			if(!impossibleDeTourner) impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse - decaleDeCentre + i + (lOrdonnee + decaleDeCentre + HAUTEUR_VOITURE)*320);
			if(!impossibleDeTourner) impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse + HAUTEUR_VOITURE + decaleDeCentre - i + (lOrdonnee + decaleDeCentre)*320);
			if(!impossibleDeTourner) impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse + HAUTEUR_VOITURE + decaleDeCentre - i + (lOrdonnee + decaleDeCentre + HAUTEUR_VOITURE)*320);
		}
		
		for(i = 0; !impossibleDeTourner && i < HAUTEUR_VOITURE; i++){
			impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse - decaleDeCentre + (lOrdonnee + decaleDeCentre + i)*320);
			if(!impossibleDeTourner) impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse + HAUTEUR_VOITURE + decaleDeCentre + (lOrdonnee + decaleDeCentre + i)*320);
  	}	
	}	
	
	// ça va dire elle veut tourner de horizontal à position vertical	
	else{
		//les deux for loop detecte la front de la voiture
		for(i = 0; !impossibleDeTourner && i < decaleDeCentre; i++){ 
			impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse + decaleDeCentre + (lOrdonnee - decaleDeCentre + i)*320);
			if(!impossibleDeTourner) impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse + decaleDeCentre + HAUTEUR_VOITURE + (lOrdonnee - decaleDeCentre + i)*320);
			if(!impossibleDeTourner) impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse + decaleDeCentre + (lOrdonnee + HAUTEUR_VOITURE + i)*320);
			if(!impossibleDeTourner) impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse + HAUTEUR_VOITURE + decaleDeCentre + (lOrdonnee + HAUTEUR_VOITURE + i)*320) ;
		}
		
		for(i = 0; !impossibleDeTourner && i < HAUTEUR_VOITURE; i++){
			impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse + decaleDeCentre + i + (lOrdonnee - decaleDeCentre)*320);
			if(!impossibleDeTourner) impossibleDeTourner = searchIdPixel(aPixelIdMur, indexAMur, lAbscisse + decaleDeCentre + i + (lOrdonnee + decaleDeCentre + HAUTEUR_VOITURE)*320);
		}	
	}
	
	return impossibleDeTourner;
}







