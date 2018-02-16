/*------------------------------*/
/* main.c -- driver pour cmcarc */
/* Auteur: Marc Gagnon          */
/*------------------------------*/

#include "cmcarc.h"


/*----------------------------------------------------------------------------*\
   Nom   : main
   But:  : Instancier cmcarc, decoder, valider et executer la requete.
   Retour: EXIT_SUCCESS ou EXIT_FAILURE.
\*----------------------------------------------------------------------------*/

int main(int argc,char **argv)
{
	cmcarc c;
	int r;

/*	sleep(100); */

	if( (c = cmcarcAllouer()) == CMCARC_NULL ) {
		(void)fprintf(stderr,"Not enough memory.\n");
		exit(EXIT_FAILURE);
	}

	if( (r=c->decoder(c,argc,(const char **)argv)) == 0 ) {
		if( c->valider(c) == 0 ) {
			if(	c->executer(c) != 0) {
			 exit(EXIT_FAILURE);
			}
		}
	}

	r = c->liberer(c);
	exit(r);
	return r;
}
