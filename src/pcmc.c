/*--------------------------------------------*/
/* pcmc.c -- implantation du protocole cmcarc */
/*--------------------------------------------*/

#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "pcmc.h"

#ifdef _LARGEFILE64_SOURCE
/* #define fseek64 fseeko
#define ftell64 ftello */
#define fseek64 fseeko64
#define ftell64 ftello64
#else
#ifdef AIX
#define fseek64 fseeko64
#define ftell64 ftello64
#endif
#endif

#define PCMC_OVERLAP 50

static char *pcmc_pos_str[] = {             /* Chaines pour enum pcmc_pos */
	 "before first byte -- nothing read"
	,"cmcarc signature"
	,"encoded file header"
	,"encoded file content"
	,"encoded file end"
};

static char *pcmc_err_str[] = {   /* Chaines pour enum pcmc_err */
	 "OK"
	,"Error"
	,"Null pointer"
	,"Not enough memory"
	,"Input/Output error"
	,"Input error"
	,"Output error"
	,"Eof detected"
	,"Bad type"
	,"Out of range"
	,"Bad position"
	,"Cannot seek"
};

static enum pcmc_err liberer       ( pcmc);
static enum pcmc_err ouvrir        ( pcmc, char*, char*);
static enum pcmc_err fileOuvrir    ( pcmc, FILE*, int);
static enum pcmc_err fermer        ( pcmc);
static enum pcmc_err signLire      ( pcmc, int*);
static enum pcmc_err signEcrire    ( pcmc);
static enum pcmc_err enteteScan    ( pcmc, long long);
static enum pcmc_err enteteLire    ( pcmc);
static enum pcmc_err enteteEcrire  ( pcmc, unsigned long long, unsigned long long, char*);
static enum pcmc_err donneesCopier ( pcmc, FILE*, unsigned char*);
static enum pcmc_err donneesEcrire ( pcmc, FILE*, long long, unsigned char*);
static enum pcmc_err donneesEcrire_input_cmcarc( pcmc , FILE*, long long, MD5_CTX*);
static enum pcmc_err donneesSauter ( pcmc);
static enum pcmc_err finLire       ( pcmc);
static enum pcmc_err finEcrire     ( pcmc, int);
static enum pcmc_err signFiEcrire  ( pcmc);
static void          msgErreur     ( pcmc, enum pcmc_err, char*, ...);
static long	     paramInfo     ( pcmc, enum pcmc_code, long);
static char*         paramInfoStr  ( pcmc, enum pcmc_code, char*); 

/*----------------------------------------------------------------------------*\
   Nom   : pcmcAllouer
   But   : Allouer une structure pcmc, l'initialiser et retourner son pointeur.
   Retour: PCME_*
\*----------------------------------------------------------------------------*/

enum pcmc_err pcmcAllouer( pcmc *ptrThis, unsigned int nk)
{
	pcmc THIS;
	size_t lbuffer;

	*ptrThis = PCMC_NULL;

	if( nk > 8192 ) {
		return PCMCE_RANGE;
	}

	THIS = (pcmc)malloc(sizeof(pcmc_a));

	if( THIS == PCMC_NULL ) {
		return PCMCE_MALLOC;
	}

	lbuffer = nk * 1024;

	if( (THIS->buffer = malloc(lbuffer)) == NULL ) {
		free(THIS);
		return PCMCE_MALLOC;
	}

	THIS->lbuffer = lbuffer;
	THIS->pos = PCMCP_NULL;
	THIS->seek = -1;
	THIS->fi = NULL;
	THIS->nfi[0] = '\0';
	THIS->sign = 0;
	THIS->xstdin = 0;
	memset(THIS->md5, '\0', 16);

	THIS->ouvrir        = ouvrir;
	THIS->liberer       = liberer;
	THIS->fileOuvrir    = fileOuvrir;
	THIS->fermer        = fermer;
	THIS->signLire      = signLire;
	THIS->signEcrire    = signEcrire;
	THIS->enteteScan    = enteteScan;
	THIS->enteteLire    = enteteLire;
	THIS->enteteEcrire  = enteteEcrire;
	THIS->donneesCopier = donneesCopier;
	THIS->donneesEcrire = donneesEcrire;
	THIS->donneesEcrire_input_cmcarc = donneesEcrire_input_cmcarc;
	THIS->donneesSauter = donneesSauter;
	THIS->finLire       = finLire;
	THIS->finEcrire     = finEcrire;
	THIS->paramInfo     = paramInfo;
	THIS->paramInfoStr  = paramInfoStr; 
	THIS->signFiEcrire  = signFiEcrire;
	
	*ptrThis = THIS;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : ouvrir
   But   : Ouvrir un fichier en format pcmc.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err ouvrir( pcmc THIS, char *nom, char *mode)
{
	if( THIS->fi != NULL ) {
		msgErreur(THIS,PCMCE_ERR,"File already open.\n");
		return PCMCE_ERR;
	}

	THIS->nfi[0] = '\0';

	if( nom == NULL  ||  mode == NULL ) {

		if( nom == NULL ) {
			msgErreur(THIS,PCMCE_NULL,
				"Cannot open file -- needs a file name.\n");
		} else {
			msgErreur(THIS,PCMCE_NULL,
				"File not opened -- needs an open mode.\n");
		}

		return PCMCE_NULL;
	}

	if( (THIS->fi = fopen64(nom,mode)) == NULL ) {
		msgErreur(THIS,PCMCE_IO, "Cannot open file, %s.\n", strerror(errno));
		return PCMCE_IO;
	}

	THIS->pos = PCMCP_SIGN;
	THIS->seek = 1;
	(void)strcpy(THIS->nfi,nom);
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : fileOuvrir
   But   : Associer un fichier "f" au protocole pcmc.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err fileOuvrir( pcmc THIS, FILE *f, int valSeek)
{
	if( THIS->fi != NULL ) {
		msgErreur(THIS,PCMCE_ERR,"File already open.\n");
		return PCMCE_ERR;
	}

	THIS->nfi[0] = '\0';

	if( f == NULL ) {
		msgErreur(THIS,PCMCE_NULL,
			"Cannot open file, FILE descriptor given is NULL.\n");
		return PCMCE_NULL;
	}

	THIS->fi = f;
	THIS->pos = PCMCP_SIGN;
	THIS->seek = valSeek;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : liberer
   But   : Liberer la memoire associee a une structure pcmc.
   Retour: PCMCE_OK
\*----------------------------------------------------------------------------*/

static enum pcmc_err liberer( pcmc THIS)
{
	free(THIS->buffer);
	free(THIS);
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : fermer
   But   : Fermer un fichier de protocole cmcarc.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err fermer( pcmc THIS)
{
	if( THIS->fi == NULL ) {
		return PCMCE_NULL;
	}

	if( fclose(THIS->fi) != 0 ) {
		msgErreur(THIS,PCMCE_IO,
			"Cannot close cmcarc file, %s.\n", strerror(errno));
	}

	THIS->pos = PCMCP_NULL;
	THIS->seek = -1;
	THIS->fi = NULL;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : signLire
   But   : Lire un signature de fichier cmcarc et valider.
   Note  : La vieille signature de 8 bytes (CMCARCHS) ne respectait pas les
         : les regles d'un fichier cmcarc interne; donc impossible a concatener
         : deux fichiers ensembles. La nouvelle signature en est donc une de
         : 32 bytes, tout comme la signature inter-fichier.
         : On doit cependant continuer a traiter les 8 premiers bytes en premier
         : lieu pour fin de compatibilite.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err signLire( pcmc THIS, int *cmcarchs_size )
{
	int n;
	char sign[24];

	if( THIS->pos != PCMCP_SIGN ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at signature position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	n = fread(sign,1,8,THIS->fi);

	if( n==8 ) {
		if( strncmp(sign,PCMC_SIGN_CMCARC_OLD,8) == 0 ) { /* vieille signature */
			*cmcarchs_size = 8;
			THIS->pos = PCMCP_ENTETE;
			cmcarc_version=VERSION4;   /* same style as v4 */
			return PCMCE_OK;

		} else { /* nouvelle signature */

			n = fread(sign,1,24,THIS->fi); /* On complete la lecture de la signature */
			if( ( n== 24 ) && strncmp(sign,PCMC_SIGN_CMCARC_OLD4,24) == 0 ) {
			     /* V4 sign */
				*cmcarchs_size = 32;
				THIS->pos = PCMCP_ENTETE;
				cmcarc_version=VERSION4;
				return PCMCE_OK;
			} else if( n==24 && strncmp(sign+8,PCMC_SIGN_CMCARC,16) == 0 ) {
			    /* v5 sign, not sure here... */
		               fread(sign, 1, 16, THIS->fi);  /* this is to read in (forget) N1+UBC */
			        *cmcarchs_size = 48; /* N1+N2+sign+N1+UBC=8+8+16+8+8 */
                                THIS->pos = PCMCP_ENTETE;	
				cmcarc_version=VERSION5;
                                return PCMCE_OK;

			} else {
				msgErreur(THIS,PCMCE_TYPE,
					"File does not begin with a cmcarc signature.\n");
				return PCMCE_TYPE;
			}
		}
	}

	if( n>0 && n <8 ) {
		msgErreur(THIS,PCMCE_TYPE,
			"File does not begin with a cmcarc signature.\n");
		return PCMCE_TYPE;
	}
	
	if( n == 0 ) {

/*              On ouvre quand meme, meme si le fichier est vide */
/*		msgErreur(THIS,PCMCE_EOF,"File empty.\n");*/
		return PCMCE_EOF;
	}

	msgErreur(THIS,PCMCE_IO,
		"Cannot read cmcarc signature, %s.\n",
		strerror(errno));
	return PCMCE_I;
}


/*----------------------------------------------------------------------------*\
   Nom   : signEcrire
   But   : Ecrire une signature de fichier cmcarc au debut du fichier.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err signEcrire( pcmc THIS)
{

	if( THIS->pos != PCMCP_SIGN ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at signature position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	THIS->pos = PCMCP_ENTETE;

	if(cmcarc_version==VERSION5) {
	  if( THIS->enteteEcrire(THIS,6,0,PCMC_SIGN_CMCARC) != PCMCE_OK ) {  
              /* nt=N1(1)+N2(1)+sign(2)+data(0)+N1(1)+UBC(1)=6 */
		msgErreur(THIS,PCMCE_O,
		"Cannot write cmcarc signature, %s.\n",
		strerror(errno));
	  }
	} else {
	   /* version 4 */
          if( THIS->enteteEcrire(THIS,4,0,PCMC_SIGN_CMCARC_OLD4) != PCMCE_OK ) {
		msgErreur(THIS,PCMCE_O,
		"Cannot write cmcarc signature, %s.\n",
		strerror(errno));
	  }
	}


	THIS->pos = PCMCP_FIN;
	if( THIS->finEcrire(THIS,0) != PCMCE_OK ) {
		msgErreur(THIS,PCMCE_ERR,
		"Last error message is from signature write function.\n");
		return PCMCE_ERR;
	}

	THIS->pos = PCMCP_ENTETE;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : enteteScan
   But   : Chercher le prochain entete valide dans un fichier cmcarc sur disque.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err enteteScan( pcmc THIS, long long pos)
{
	char *p,*buffer,*ebuffer;
	long long npos;
	int lu;

	if( !THIS->seek ) {
		return PCMCE_ERR;
	}

	buffer = (char *)THIS->buffer;
        npos = pos;

	do {

		if( fseek64(THIS->fi,npos,SEEK_SET) == -1 ) {
			return PCMCE_SEEK;
		}

		pos = ftell64(THIS->fi);
		lu = fread(buffer,1,THIS->lbuffer,THIS->fi);

		if( ferror(THIS->fi) ) {
			return PCMCE_I;
		}

		npos = pos + lu - PCMC_OVERLAP;

		ebuffer = buffer + lu - 1;
		p = buffer;

		while( p <= ebuffer ) {

			if( *p == PCMCC_NOMF ) {

				/* Signature inter-fichier*/
				if( strncmp(p,PCMC_SIGN_IFI_CMCARC,24) == 0 ) {
					long long new_pos;
					if(cmcarc_version==VERSION4)
					  new_pos=pos+(p-buffer)+24+8; 
					else
					  new_pos=pos+(p-buffer)+24+16;  /* v5 */

					/* if( fseek64(THIS->fi,(long long)pos+p-buffer+24+8,SEEK_SET) == -1 ) { */	
					if( fseek64(THIS->fi,new_pos,SEEK_SET) == -1 ) {
						return PCMCE_SEEK;
					}

					THIS->sign = 1;
					return PCMCE_OK;
				}
			   	/* no check for version 3, it couldn't be concatenated */
				/* Signature d'un fichier cmcarc v4 */
				if( strncmp(p,PCMC_SIGN_CMCARC_OLD4,16) == 0 ) {
					long long new_pos=pos+(p-buffer)+16+8;


					/* if( fseek64(THIS->fi,(long long)pos+p-buffer+16+8,SEEK_SET) == -1 ) { */
					if( fseek64(THIS->fi,new_pos,SEEK_SET) == -1 ) {
						return PCMCE_SEEK;
					}

					THIS->sign = 1;
					cmcarc_version=VERSION4;
					return PCMCE_OK;
				}
				/* Signature d'un fichier cmcarc v5 */
                                if( strncmp(p,PCMC_SIGN_CMCARC,16) == 0 ) {
                                        /* long long new_pos=pos+(p-buffer)+16+16;   */
                                        long long new_pos=pos+(p-buffer)+16+8+8;  /* sign(16)+N1(8)+UBC(8) */ 

                                        if( fseek64(THIS->fi,new_pos,SEEK_SET) == -1 ) {
                                                return PCMCE_SEEK;
                                        }

                                        THIS->sign = 1;
					cmcarc_version=VERSION5;
                                        return PCMCE_OK;
                                }

			}
			p++;

		}

	} while( lu > PCMC_OVERLAP );

	/* Si on arrive a la fin sans avoir identifie de signature... */

	return PCMCE_EOF;
}


/*----------------------------------------------------------------------------*\
   Nom   : enteteLire
   But   : Lire un entete d'un fichier cmcarc.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err enteteLire( pcmc THIS)
{
	int n;
	unsigned char word[8], block[32];

	if( THIS->pos != PCMCP_ENTETE ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at header position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	if(!THIS->xstdin) {
	  /* note: if we read in from stdin, we can't rewind.  Therefore, we cannot 
	   * support mix versioned cmcarc file 					    */
	
          if(fread(block,1,32,THIS->fi)!=32) {
            if( feof(THIS->fi) ) {
              return PCMCE_EOF;
            }

            msgErreur(THIS,PCMCE_IO,
                      "Cannot read first part of header, %s.\n",strerror(errno));
              return PCMCE_I;
          }


	  if(strncmp((const char *)block+8,PCMC_SIGN_CMCARC_OLD4,16) == 0 ) {
	    /* we are switching to/continuing with v4 */
	    cmcarc_version=VERSION4;
	  } else if(strncmp((const char *)block+16,PCMC_SIGN_CMCARC, 16) == 0 ) {
	    /* we are switching to/continuing with v5 */
            cmcarc_version=VERSION5;
	  }
	  /* else it is not a sign, go on */

	  /* rewind */
	  fseek64(THIS->fi, -32, SEEK_CUR); 
	}

	if(cmcarc_version==VERSION5) {
	  /* version 5 */

	  if(fread(word,1,8,THIS->fi)!=8) {
            if( feof(THIS->fi) ) {
                return PCMCE_EOF;
            }

            msgErreur(THIS,PCMCE_IO,
                        "Cannot read first part of header, %s.\n",strerror(errno));
                return PCMCE_I;
          }

	  THIS->nbr_total =
		(word[0] << 24) |
		(word[1] << 16) |
		(word[2] << 8 ) |
                (word[3]      );

	  THIS->nbr_total = THIS->nbr_total << 32;
	  
	  THIS->nbr_total = THIS->nbr_total |
		(word[4] << 24) |
                (word[5] << 16) |
                (word[6] << 8 ) |
                (word[7]      );

	  if(fread(word,1,8,THIS->fi)!=8) {
            if( feof(THIS->fi) ) {
                return PCMCE_EOF;
            }

            msgErreur(THIS,PCMCE_IO,
                        "Cannot read first part of header, %s.\n",strerror(errno));
                return PCMCE_I;
          }

	
	  THIS->nbr_donnees =
		(word[0] << 24) |
		(word[1] << 16) |
		(word[2] << 8 ) |
                (word[3]      );
		
	  THIS->nbr_donnees = THIS->nbr_donnees << 32;

	  THIS->nbr_donnees = THIS->nbr_donnees |
	  	(word[4] << 24) |
                (word[5] << 16) |
                (word[6] << 8 ) |
                (word[7]      );
	  
	  THIS->nbr_header = ((unsigned int)(THIS->nbr_total - THIS->nbr_donnees) - 4) *8; 
	} else {
	  /* version 4 */
	  unsigned int nt, nd;      /* 32 bits */

	  if(fread(word,1,8,THIS->fi)!=8) {
            if( feof(THIS->fi) ) {
                return PCMCE_EOF;
            }

            msgErreur(THIS,PCMCE_IO,
                        "Cannot read first part of header, %s.\n",strerror(errno));
                return PCMCE_I;
          }

          nt =
                (word[0] << 24) |
                (word[1] << 16) |
                (word[2] << 8 ) |
                (word[3]      ) ;

	  nd =
                (word[4] << 24) |
                (word[5] << 16) |
                (word[6] << 8 ) |
                (word[7]      ) ;

	  THIS->nbr_total=nt;
	  THIS->nbr_donnees=nd;
	  THIS->nbr_header = ((unsigned int)(THIS->nbr_total - THIS->nbr_donnees) - 2) * 8;
	}

	if( THIS->nbr_donnees > THIS->nbr_total - 3 ) {
		msgErreur(THIS,PCMCE_RANGE,
			"Bad header indicates more data words than possible (%llu>%llu).\n",
			THIS->nbr_donnees,THIS->nbr_total-3);
		return PCMCE_RANGE;
	}

	if( THIS->nbr_header > PCMC_MAXPARAM ) {
		msgErreur(THIS,PCMCE_RANGE,
			"Parameter in header too long (%d) to be read, maximum is %d.\n",
			THIS->nbr_header,PCMC_MAXPARAM);
		return PCMCE_RANGE;
	}

	n = fread(THIS->entete.param,1,THIS->nbr_header,THIS->fi);

	if( n != THIS->nbr_header ) {
		msgErreur(THIS,PCMCE_IO,
			"Cannot read parameters in header, %s.\n",strerror(errno));
		return PCMCE_I;
	}

        THIS->entete.param[n] = '\0';

	THIS->pos = PCMCP_DONNEES;

	if( THIS->debug ) {
		(void)fprintf(stdout,"enteteLire: nt=%llu nd=%llu nom=%s\n",
			THIS->nbr_total,THIS->nbr_donnees,THIS->entete.param+1);
	}

	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : enteteEcrire
   But   : Ecrire un entete de fichier cmcarc a l'aide des informations passees.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err enteteEcrire( pcmc THIS, unsigned long long nt, unsigned long long nd, char *param)
{
        unsigned char word[8];   /* place holder for a number */

	if( THIS->pos != PCMCP_ENTETE ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at header position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	THIS->nbr_total = nt;
	THIS->nbr_donnees = nd;
/*
	THIS->fin.nbr_total[0] = THIS->entete.nbr_total[0] = (pcmc_uchar) ((nt >> 24) & 255);
	THIS->fin.nbr_total[1] = THIS->entete.nbr_total[1] = (pcmc_uchar) ((nt >> 16) & 255);
	THIS->fin.nbr_total[2] = THIS->entete.nbr_total[2] = (pcmc_uchar) ((nt >> 8 ) & 255);
	THIS->fin.nbr_total[3] = THIS->entete.nbr_total[3] = (pcmc_uchar) ((nt      ) & 255);

	THIS->entete.nbr_donnees[0] = (pcmc_uchar) ((nd >> 24) & 255);
	THIS->entete.nbr_donnees[1] = (pcmc_uchar) ((nd >> 16) & 255);
	THIS->entete.nbr_donnees[2] = (pcmc_uchar) ((nd >> 8 ) & 255);
	THIS->entete.nbr_donnees[3] = (pcmc_uchar) ((nd      ) & 255);
*/

	if(cmcarc_version==VERSION5) {
		THIS->nbr_header = (nt - nd - 4) * 8; 
		  

		/* to insure big endian notation */
		word[0] = (pcmc_uchar) ((nt >> 56) & 255);
		word[1] = (pcmc_uchar) ((nt >> 48) & 255);
		word[2] = (pcmc_uchar) ((nt >> 40) & 255);
        	word[3] = (pcmc_uchar) ((nt >> 32) & 255);
		word[4] = (pcmc_uchar) ((nt >> 24) & 255);
        	word[5] = (pcmc_uchar) ((nt >> 16) & 255);
        	word[6] = (pcmc_uchar) ((nt >> 8 ) & 255);
        	word[7] = (pcmc_uchar) ((nt      ) & 255);

		if(fwrite(word, 1, 8, THIS->fi)!=8) {
	  		msgErreur(THIS,PCMCE_IO,
                        "Cannot write encoded file's header, %s.\n",strerror(errno));
                   return PCMCE_O;
        	}

		word[0] = (pcmc_uchar) ((nd >> 56) & 255);
        	word[1] = (pcmc_uchar) ((nd >> 48) & 255);
        	word[2] = (pcmc_uchar) ((nd >> 40) & 255);
        	word[3] = (pcmc_uchar) ((nd >> 32) & 255);
        	word[4] = (pcmc_uchar) ((nd >> 24) & 255);
        	word[5] = (pcmc_uchar) ((nd >> 16) & 255);
        	word[6] = (pcmc_uchar) ((nd >> 8 ) & 255);
        	word[7] = (pcmc_uchar) ((nd      ) & 255);

        	if(fwrite(word, 1, 8, THIS->fi)!=8) {
          		msgErreur(THIS,PCMCE_IO,
                        "Cannot write encoded file's header, %s.\n",strerror(errno));
                   return PCMCE_O;
        	}
	} else {
	   /* version 4 */
		THIS->nbr_header = (nt - nd - 2) * 8; 
		
		word[0] = (pcmc_uchar) ((nt >> 24) & 255);
        	word[1] = (pcmc_uchar) ((nt >> 16) & 255);
        	word[2] = (pcmc_uchar) ((nt >> 8 ) & 255);
        	word[3] = (pcmc_uchar) ((nt      ) & 255);
		
		if(fwrite(word, 1, 4, THIS->fi)!=4) {
	  		msgErreur(THIS,PCMCE_IO,
                        "Cannot write encoded file's header, %s.\n",strerror(errno));
                   return PCMCE_O;
        	}
        	
		word[0] = (pcmc_uchar) ((nd >> 24) & 255);
        	word[1] = (pcmc_uchar) ((nd >> 16) & 255);
        	word[2] = (pcmc_uchar) ((nd >> 8 ) & 255);
        	word[3] = (pcmc_uchar) ((nd      ) & 255);

        	if(fwrite(word, 1, 4, THIS->fi)!=4) {
          		msgErreur(THIS,PCMCE_IO,
                        "Cannot write encoded file's header, %s.\n",strerror(errno));
                   return PCMCE_O;
        	}
	}

        memset(THIS->entete.param, '\0', PCMC_MAXPARAM);  /*  why is this? */
        /* strcpy(THIS->entete.param, param);  it could have a '\0' in the middle */
	memcpy(THIS->entete.param, param, THIS->nbr_header);
	if(fwrite(THIS->entete.param, 1, THIS->nbr_header, THIS->fi) != THIS->nbr_header) { 
	  msgErreur(THIS,PCMCE_IO,
                        "Cannot write encoded file's header, %s.\n",strerror(errno));
                return PCMCE_O;
        }

	THIS->pos = PCMCP_DONNEES;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : donneesCopier
   But   : Copier une section donnees d'un fichier cmcarc vers un fichier.
   	   If md5 is not null, compute (and return) md5
   Retour: PCME_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err donneesCopier( pcmc THIS, FILE *ou, unsigned char* md5)
{
	/* size_t n,nb,nc,tot; */
	unsigned long long n, nc, tot;
	size_t nb;

	char buff[24];
	MD5_CTX md5_ctx;

        int offset=0;  /* this little trick tells us where to check for ub
			depending the existence of cksum */

	if( THIS->pos != PCMCP_DONNEES ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at content position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	if(md5)
	  MD5_Init(&md5_ctx);

/* CASES:
 * if nd==0 : empty file, no cksum
 *    0<nd<3 : no cksum
 *    nd>=3 && ubc<16 : no cksum
 *    nd>=3 && ubc>=16: cksum
 */
	if( THIS->nbr_donnees > 0 ) {
		/* size_t is supposed to be an unsigned int at -n32 */
		n = (unsigned long long) (THIS->nbr_donnees - 1) * 8;
		if(THIS->nbr_donnees>=3) 
		  n-=16;   /* skip the last 16 bytes (it may be cksum) */
		else
		  offset=0;

		tot = n;

		while( n != 0 ) { 
			nc = ( n > THIS->lbuffer ) ? THIS->lbuffer : n;

			if( fread(THIS->buffer,1,nc,THIS->fi) != nc ) {
				msgErreur(THIS,PCMCE_IO,
					"Cannot read from cmcarc file after %llu bytes, %s.\n",
					tot-n,strerror(errno));
				return PCMCE_I;
			}

			if(md5) 
			  MD5_Update(&md5_ctx, THIS->buffer, nc);


			if( fwrite((void *)THIS->buffer,1,nc,ou) != nc ) {
				msgErreur(THIS,PCMCE_IO,
					"Cannot write buffer to file after %llu bytes, %s.\n",
					tot-n,strerror(errno));
				return PCMCE_O;
			}

			n -= nc;
		}
	
		/* On lit les derniers 24 octets separement car une partie
		   peut etre inutile (unused bits/cksum). */

		if(THIS->nbr_donnees>=3) 
			nc=24;
		else
			nc=8;   /* too small for cksum, read ubc only */

		if( fread(buff,1,nc,THIS->fi) != nc) {
			msgErreur(THIS,PCMCE_O,
				"Cannot read data just before unused bits, %s.\n",
				strerror(errno));
			return PCMCE_I;
		}

	}

	THIS->pos = PCMCP_FIN;

	if( finLire(THIS) != PCMCE_OK ) {
		msgErreur(THIS,PCMCE_I,
			"Last error occured while copying data before unused bits.\n");
		return PCMCE_I;
	}

	/* check the "possible" cksum (16 bytes) */

	if(THIS->nbr_unused>=128) {
	   /* the last 16 bytes of "buff" is cksum */
	   /* UBC is in bits, not bytes!  grrr! */
           memcpy(THIS->md5, buff+8, 16);
	   /* this is cksum from input data, NOT computed */
	   offset=0;
	} else if( THIS->nbr_donnees >= 3 ){
	   /* no cksum from the origin, the (before) last 16 bytes is thf part of data */
	   if( fwrite(buff,1,16,ou) != 16 ) {
	       msgErreur(THIS,PCMCE_IO,
		  "Cannot write buffer to file after %llu bytes, %s.\n",
		  tot-16,strerror(errno));
		return PCMCE_O;
	   }
           offset=16;
	   if(md5)
	     MD5_Update(&md5_ctx, buff, 16);
	}

	/* check the unused bits (last 8 bytes) */
	if(md5)  {
	  MD5_Update(&md5_ctx, buff+offset, 8-((THIS->nbr_unused/8)%8)); 
	  MD5_Final(md5, &md5_ctx);
	}


	/* Connaissant nu, on sait maintenant combien d'octets il faut copier */
	if( THIS->nbr_donnees > 0 ) {
		/* nb = 8-THIS->nbr_unused/8; */
		nb = 8-((THIS->nbr_unused/8)%8);
		if( fwrite(buff+offset,1,nb,ou) != nb ) {
			msgErreur(THIS,PCMCE_O,
				"Cannot write data just before unused bits, %s.\n",
				strerror(errno));
			return PCMCE_O;
		}
	}

	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : donneesEcrire
   But   : Ecrire la section donnees d'un fichier cmcarc d'un fichier ouvert.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err donneesEcrire( pcmc THIS, FILE *in, long long n, unsigned char* md5)
{
	/* size_t nu,nc,tot; */
	long long nc, tot;
	size_t nu;
	MD5_CTX md5_ctx;

	if( THIS->pos != PCMCP_DONNEES ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at content position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	if(md5)
	  MD5_Init(&md5_ctx);

	tot = n;

	while( n != 0 ) {

		nc = ( n > THIS->lbuffer ) ? THIS->lbuffer : n;

		if( (fread(THIS->buffer,1,nc,in)) != nc ) {
			msgErreur(THIS,PCMCE_I,
				"Cannot read from input file after %ld bytes, %s.\n",
				(long long) tot-n,strerror(errno));
			return PCMCE_I;
		}

		if(md5) 
		  MD5_Update(&md5_ctx, THIS->buffer, nc);

		if( fwrite((void *)THIS->buffer,1,nc,THIS->fi) != nc ) {
			msgErreur(THIS,PCMCE_O,
			"Cannot write buffer to cmcarc file after %ld bytes, %s.\n",
			(long long)tot-n,strerror(errno));
			return PCMCE_O;
		}

		n -= nc;
	}

	nu = (size_t) (tot % 8) ? 8-(tot % 8) : 0;


	if( tot && nu ) {
		if( fwrite((void *)"\0\0\0\0\0\0\0",1,nu,THIS->fi) != nu ) { 
			msgErreur(THIS,PCMCE_O,
				"Cannot write padding for unused bits in cmcarc file, %s.\n",
				strerror(errno));
			return PCMCE_O;
		}
	}

	if(md5) { 
	  MD5_Final(md5, &md5_ctx);

	  /* write checksum info (md5) */
          if(fwrite(THIS->md5, 1, 16, THIS->fi)!=16) {
	    msgErreur(THIS,PCMCE_O,
                        "Can't write checksum (md5), %s.\n",
                        strerror(errno));
                return PCMCE_O;
          }

	}

	THIS->pos = PCMCP_FIN;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : donneesEcrire_input_cmcarc
   But   : Ecrire la section donnees d'un fichier cmcarc d'un fichier ouvert.
   Note: This is a different version of donneesEcrire, where the input comes
   	 from a cmcarc file rather then a regular file.  This is because of the
	 unused bit count screws up the checksum info...  This version is
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err donneesEcrire_input_cmcarc( pcmc THIS, FILE *in, long long n, MD5_CTX* md5_ctx)
{
	/* size_t nu,nc,tot; */
	long long nc, tot;

	if( THIS->pos != PCMCP_DONNEES ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at content position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	tot = n;

	while( n != 0 ) {

		nc = ( n > THIS->lbuffer ) ? THIS->lbuffer : n;

		if( (fread(THIS->buffer,1,nc,in)) != nc ) {
			msgErreur(THIS,PCMCE_I,
				"Cannot read from input file after %ld bytes, %s.\n",
				(long long) tot-n,strerror(errno));
			return PCMCE_I;
		}

		if(md5_ctx) 
		  MD5_Update(md5_ctx, THIS->buffer, nc);

		if( fwrite((void *)THIS->buffer,1,nc,THIS->fi) != nc ) {
			msgErreur(THIS,PCMCE_O,
			"Cannot write buffer to cmcarc file after %ld bytes, %s.\n",
			(long long)tot-n,strerror(errno));
			return PCMCE_O;
		}

		n -= nc;
	}


	THIS->pos = PCMCP_FIN;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : donneesSauter
   But   : Avancer apres les donnees et les informations de fin de fichier.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err donneesSauter( pcmc THIS)
{
	if( THIS->pos != PCMCP_DONNEES ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at content position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	if( THIS->seek ) {
#ifndef AIX
		off_t nd_size=(off_t)THIS->nbr_donnees*8;
#else
		long long nd_size=(long long)THIS->nbr_donnees*8;
#endif
		if(nd_size>16) {
		        /* there is at least enough data to contain a cksum ;) */
		        if( fseek64(THIS->fi,nd_size-16,SEEK_CUR) != 0 ) {
			    msgErreur(THIS,PCMCE_SEEK,
				"Cannot skip data in cmcarc file, %s. : %d SEEK_CUR=%d, nd=%ld\n",
				strerror(errno), errno, SEEK_CUR, THIS->nbr_donnees);
			    return PCMCE_SEEK;
		        }

			/* last 16 bytes _may_ be the cksum */
                        if(fread(THIS->md5,1,16,THIS->fi)!=16) {
			   msgErreur(THIS,PCMCE_I,
				"Cannot read end of file, %s.\n", strerror(errno));
			   return PCMCE_I;
			}
		} else {
		       /* not enough data for md5 */
		       /* strcpy(THIS->md5, "N/A");   -* ??? */

		       if( fseek64(THIS->fi,nd_size,SEEK_CUR) != 0 ) {
				msgErreur(THIS,PCMCE_SEEK,
				"Cannot skip data in cmcarc file, %s. : %d SEEK_CUR=%d, nd=%ld\n",
				strerror(errno), errno, SEEK_CUR, THIS->nbr_donnees);
			  return PCMCE_SEEK;
		       }
                }

		THIS->pos = PCMCP_FIN;

		if( finLire(THIS) != PCMCE_OK ) {
			msgErreur(THIS,PCMCE_I,
				"Last error occured right after skipping data by seeking.\n");
			return PCMCE_I;
		}

		if(THIS->nbr_unused<128) {
		  /* md5 wasn't recorded */
		  memset(THIS->md5, '\0', 16);  
		}

	} else {

		/* Normalement, on arrive ici quand l'entree est stdin */

		FILE *f;

		if( (f=fopen("/dev/null","w")) == NULL ) {
			msgErreur(THIS,PCMCE_IO,
				"Cannot open /dev/null to skip data, %s.\n",
				strerror(errno));
			return PCMCE_IO;
		}

		if( THIS->donneesCopier(THIS,f,NULL) != PCMCE_OK ) {
			msgErreur(THIS,PCMCE_I,
				"Last error occured right after skipping data by scanning.\n");
			return PCMCE_I;
		}

		if( fclose(f) != 0 ) {
			msgErreur(THIS,PCMCE_IO,
				"Cannot close /dev/null file, %s\n",
				strerror(errno));
		}
	}

	THIS->pos = PCMCP_ENTETE;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : finLire
   But   : Lire les informations de fin de fichier apres les "data".
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err finLire( pcmc THIS)
{
	unsigned long long nt;
	unsigned char word[8];

	if( THIS->pos != PCMCP_FIN ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at end position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

        if(cmcarc_version==VERSION5) {
	  #if 0
          /* version 5 */
          if(fread(THIS->md5,1,16,THIS->fi)!=16) {
	    msgErreur(THIS,PCMCE_I,
                        "Cannot read end of file, %s.\n",
                        strerror(errno));
                return PCMCE_I;
          }
          #endif

          if(fread(word,1,8,THIS->fi)!=8) {
	    msgErreur(THIS,PCMCE_I,
                        "Cannot read end of file, %s.\n",
                        strerror(errno));
                return PCMCE_I;
          }
          nt =
                (word[0] << 24) |
                (word[1] << 16) |
                (word[2] << 8 ) |
                (word[3]      );

	  nt = nt << 32;

	  nt = nt |
                (word[4] << 24) |
                (word[5] << 16) |
                (word[6] << 8 ) |
                (word[7]      );

          if(fread(word,1,8,THIS->fi)!=8) {
	    msgErreur(THIS,PCMCE_I,
                        "Cannot read end of file, %s.\n",
                        strerror(errno));
                return PCMCE_I;
          }

	  THIS->nbr_unused =
                (word[0] << 24) |
                (word[1] << 16) |
                (word[2] <<  8) |
                (word[3]      );

	  THIS->nbr_unused = THIS->nbr_unused << 32;

	  THIS->nbr_unused = THIS->nbr_unused |
                (word[4] << 24) |
                (word[5] << 16) |
                (word[6] << 8 ) |
                (word[7]      );

        } else {
          /* version 4 */
          unsigned int nbr_t, nbr_u;      /* 32 bits */
          if(fread(word,1,8,THIS->fi)!=8) {
	    msgErreur(THIS,PCMCE_I,
                        "Cannot read end of file, %s.\n",
                        strerror(errno));
                return PCMCE_I;
          }
          nbr_t =
                (word[0] << 24) |
                (word[1] << 16) |
                (word[2] << 8 ) |
                (word[3]      ) ;

          nbr_u =
                (word[4] << 24) |
                (word[5] << 16) |
                (word[6] << 8 ) |
                (word[7]      ) ;

	  nt=nbr_t;
          THIS->nbr_unused=nbr_u;
        }


	if( nt != THIS->nbr_total ) {
		msgErreur(THIS,PCMCE_ERR,
			"Total length read at end doesn't match first one, %llu != %llu.\n",
			nt,THIS->nbr_total);
		return PCMCE_ERR;
	}

	/*
	if( THIS->nbr_unused > 64 ) {
		msgErreur(THIS,PCMCE_RANGE,
			"Mode than 64 (%d) unused bits, impossible.\n",THIS->nbr_unused);
		return PCMCE_RANGE;
	}
        */
	THIS->pos = PCMCP_ENTETE;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : finEcrire
   But   : Ecrire les informations de fin de fichier apres les "data".
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err finEcrire( pcmc THIS, int nu)
{
	unsigned char word[8];

	if( THIS->pos != PCMCP_FIN ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at end position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	/* Well, it will be possible after hiding md5 in UBC -
	if( nu > 64 ) {
		msgErreur(THIS,PCMCE_RANGE,
			"Mode than 64 (%d) unused bits, impossible.\n",nu);
		return PCMCE_RANGE;
	} */


	#if 0  /* this is part of donneesEcrire now */
	/* write checksum info (md5) */
        if(fwrite(THIS->md5, 1, 16, THIS->fi)!=16) {
	  msgErreur(THIS,PCMCE_O,
                        "Can't write checksum (md5), %s.\n",
                        strerror(errno));
                return PCMCE_O;
        }
        #endif

	if(cmcarc_version==VERSION5) {
          	/* to insure big endian notation */
          	word[0] = (pcmc_uchar) ((THIS->nbr_total >> 56) & 255);
        	word[1] = (pcmc_uchar) ((THIS->nbr_total >> 48) & 255);
        	word[2] = (pcmc_uchar) ((THIS->nbr_total >> 40) & 255);
        	word[3] = (pcmc_uchar) ((THIS->nbr_total >> 32) & 255);
        	word[4] = (pcmc_uchar) ((THIS->nbr_total >> 24) & 255);
        	word[5] = (pcmc_uchar) ((THIS->nbr_total >> 16) & 255);
        	word[6] = (pcmc_uchar) ((THIS->nbr_total >> 8 ) & 255);
        	word[7] = (pcmc_uchar) ((THIS->nbr_total      ) & 255);

        	if(fwrite(word, 1, 8, THIS->fi)!=8) {
	  		msgErreur(THIS,PCMCE_O,
                        "Cannot write end informations (nt and nu), %s.\n",
                        strerror(errno));
                   return PCMCE_O;
        	}

		THIS->nbr_unused = nu;

		word[0] = (pcmc_uchar) ((THIS->nbr_unused >> 56) & 255);
        	word[1] = (pcmc_uchar) ((THIS->nbr_unused >> 48) & 255);
        	word[2] = (pcmc_uchar) ((THIS->nbr_unused >> 40) & 255);
        	word[3] = (pcmc_uchar) ((THIS->nbr_unused >> 32) & 255);
        	word[4] = (pcmc_uchar) ((THIS->nbr_unused >> 24) & 255);
        	word[5] = (pcmc_uchar) ((THIS->nbr_unused >> 16) & 255);
        	word[6] = (pcmc_uchar) ((THIS->nbr_unused >> 8 ) & 255);
        	word[7] = (pcmc_uchar) ((THIS->nbr_unused      ) & 255);

        	if(fwrite(word, 1, 8, THIS->fi)!=8) {
	  		msgErreur(THIS,PCMCE_O,
                        "Cannot write end informations (nt and nu), %s.\n",
                        strerror(errno));
                   return PCMCE_O;
        	}
	} else {

        	word[0] = (pcmc_uchar) ((THIS->nbr_total >> 24) & 255);
        	word[1] = (pcmc_uchar) ((THIS->nbr_total >> 16) & 255);
        	word[2] = (pcmc_uchar) ((THIS->nbr_total >> 8 ) & 255);
        	word[3] = (pcmc_uchar) ((THIS->nbr_total      ) & 255);

        	if(fwrite(word, 1, 4, THIS->fi)!=4) {
	  		msgErreur(THIS,PCMCE_O,
                        "Cannot write end informations (nt and nu), %s.\n",
                        strerror(errno));
                   return PCMCE_O;
        	}

		THIS->nbr_unused = nu;

        	word[0] = (pcmc_uchar) ((THIS->nbr_unused >> 24) & 255);
        	word[1] = (pcmc_uchar) ((THIS->nbr_unused >> 16) & 255);
        	word[2] = (pcmc_uchar) ((THIS->nbr_unused >> 8 ) & 255);
        	word[3] = (pcmc_uchar) ((THIS->nbr_unused      ) & 255);

        	if(fwrite(word, 1, 4, THIS->fi)!=4) {
	  		msgErreur(THIS,PCMCE_O,
                        "Cannot write end informations (nt and nu), %s.\n",
                        strerror(errno));
                   return PCMCE_O;
        	}
	}

	THIS->pos = PCMCP_ENTETE;
	return PCMCE_OK;
}




/*----------------------------------------------------------------------------*\
   Nom   : signFiEcrire
   But   : Ecrire une signature inter-fichier.
   Retour: PCMCE_*
\*----------------------------------------------------------------------------*/

static enum pcmc_err signFiEcrire( pcmc THIS)
{
	if( THIS->pos != PCMCP_ENTETE ) {
		msgErreur(THIS,PCMCE_POS,
			"Not at header position, current position is %s.\n",
			pcmc_pos_str[THIS->pos]);
		return PCMCE_POS;
	}

	if(cmcarc_version==VERSION5) {
	    if( THIS->enteteEcrire(THIS,7,0,PCMC_SIGN_IFI_CMCARC) != PCMCE_OK ) {
            /* nt=N1(1)+N2(1)+sign_fichier(3)+data(0)+N1(1)+UBC(1)=7 */
		msgErreur(THIS,PCMCE_ERR,
		"Last error message is from inter-file signature write function.\n");
		return PCMCE_ERR;
	    }
	} else {
	    /* version 4 */
	    if( THIS->enteteEcrire(THIS,5,0,PCMC_SIGN_IFI_CMCARC) != PCMCE_OK ) { 
		msgErreur(THIS,PCMCE_ERR,
		"Last error message is from inter-file signature write function.\n");
		return PCMCE_ERR;
	    }
	}

	THIS->pos = PCMCP_FIN;

	if( THIS->finEcrire(THIS,0) != PCMCE_OK ) {
		msgErreur(THIS,PCMCE_ERR,
		"Last error message is from inter-file signature write function.\n");
		return PCMCE_ERR;
	}

	THIS->pos = PCMCP_ENTETE;
	return PCMCE_OK;
}


/*----------------------------------------------------------------------------*\
   Nom   : paramInfo
   But   : Retourner une information du champ parameter du fichier courant.
   Retour: L'information si present ou le defaut de l'appel.
\*----------------------------------------------------------------------------*/

static long paramInfo( pcmc THIS, enum pcmc_code info, long defaut )
{
        int i,j;
        long long valeur;
	unsigned char *p;
	
	valeur = 0;
	p = (unsigned char *) (THIS->entete.param);

	while ( (unsigned int)( p - (unsigned char *)(THIS->entete.param) + 1 ) < THIS->nbr_header ) {
		if( p[0] == info ) {
		   p++;
		   if(info==PCMCC_MTIME) {
		        /* special case, 'cause it could contain a null byte 
			 * in string */
		        for(i=3, j=0; i>=0; i--, j++) 
				valeur += p[j] << (i * 8);

		    } else {
			for(i = strlen((char *)(p)) - 1, j = 0; i >= 0; i--, j++) {
				valeur += p[j] << (i * 8);
			}
		    }
		    return valeur;
		}
		p += strlen((char *)(p)) + 1;
	}
        
	return defaut;
}

/*----------------------------------------------------------------------------*\
   Nom   : paramInfoStr
   But   : Retourner une information du champ parameter du fichier courant.
   	   (en format string)
   Retour: L'information si present ou le defaut de l'appel.
\*----------------------------------------------------------------------------*/

static char* paramInfoStr( pcmc THIS, enum pcmc_code info, char* defaut )    
{
	char* value;
	unsigned char *p;
	
	p = (unsigned char *) (THIS->entete.param);

	while ( (unsigned int)( p - (unsigned char *)(THIS->entete.param) + 1 ) < THIS->nbr_header ) {
		if( p[0] == info ) {
			p++;
			value = strdup((const char *)p);   /* terminates at the '\0' */
			return value;
		}
		p += strlen((char *)(p)) + 1;
	}
        
	return defaut;
}

/*----------------------------------------------------------------------------*\
   Nom   : msgErreur
   But   : Formatter et afficher un message d'erreur du protocole pcmc.
\*----------------------------------------------------------------------------*/

static void msgErreur( pcmc THIS, enum pcmc_err no, char* fmt, ...)
{
	char ch[500];
	va_list ap;

	va_start(ap,fmt);
	(void)vsprintf(ch,fmt,ap);

	if( THIS->nfi[0] == '\0' ) {
		(void)fprintf(stderr,"STDIN/OUT: Error %d (%s) %s",
			no,pcmc_err_str[no],ch);
	} else {
		(void)fprintf(stderr,"%s: Error %d (%s) %s",
			THIS->nfi,no,pcmc_err_str[no],ch);
	}

	va_end(ap);
}
