/*-----------------------------------------*/
/* pcmc.h -- interface du protocole cmcarc */
/*-----------------------------------------*/

#ifndef PCMC_H
#define PCMC_H

#include <stdio.h>
#include <stdlib.h>
#include <openssl/md5.h>

enum version_type { VERSION4, VERSION5 };

/* static */ enum version_type cmcarc_version;   /* denotes which version of cmcarc file we are
					 reading.  Note that it can change within a file because
					 of concatenation  */

/* Longueur maximum de la section des parametres d'un fichier */

#define PCMC_MAXPARAM 5000

/* Signature identifiant un fichier cmcarc */

#define PCMC_SIGN_CMCARC      "0CMCARCH5\0\0\0\0\0\0\0"
#define PCMC_SIGN_CMCARC_OLD4 "0CMCARCHS\0\0\0\0\0\0\0"      /* Version 4 */ 
#define PCMC_SIGN_CMCARC_OLD  "CMCARCHS"		     /* version 3 */

/* Signature inter-fichier (respecte les regles d'un fichier cmcarc normal */
#define PCMC_SIGN_IFI_CMCARC "0signature_fichier\0\0\0\0\0\0"

/* if no --md5 is specified, checksum is NULL */
#define MD5_NULL	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"

typedef unsigned char pcmc_uchar;

/* Positions logiques dans un fichier en format cmcarc */

enum pcmc_pos {
	 PCMCP_NULL     /* Indefini */
	,PCMCP_SIGN     /* Debut du fichier cmcarc */
	,PCMCP_ENTETE   /* Entete precedant les donnees */
	,PCMCP_DONNEES  /* Donnees d'un fichier apres l'entete */
	,PCMCP_FIN      /* Informations de fin de fichier, apres donnees */
};

/* Codes d'erreur retournes par les fonctions */

enum pcmc_err {
	 PCMCE_OK       /* Pas d'erreur */
	,PCMCE_ERR      /* Erreur generique */
	,PCMCE_NULL     /* Pointeur NULL */
	,PCMCE_MALLOC   /* Manque de memoire */
	,PCMCE_IO       /* Probleme en entree/sortie (ex: feof,fclose) */
	,PCMCE_I        /* Probleme en entree */
	,PCMCE_O        /* Probleme en sortie */
	,PCMCE_EOF      /* Fin de fichier */
	,PCMCE_TYPE     /* Mauvais type de fichier ou signature attendue */
	,PCMCE_RANGE    /* Valeur non-valide, trop petit ou trop grand */
	,PCMCE_POS      /* Mauvaise position logique dans le fichier cmcarc */
	,PCMCE_SEEK     /* Probleme en faisant un fseek */
};

/* Codes dans un en-tete de fichier */

enum pcmc_code {
	 PCMCC_NOMF  = '0'  /* Nom du fichier */
	,PCMCC_MODE  = '1'  /* Mode (permissions) */
	,PCMCC_UID   = '2'  /* uid (usager proprietaire) */
	,PCMCC_GID   = '3'  /* gid (groupe proprietaire) */
	,PCMCC_MTIME = '4'  /* mtime (date de modification) */
	,PCMCC_LINK  = '5'  /* link target */
};

/* Representation d'un fichier cmcarc (attributs et methodes) */

typedef struct pcmc_s {

	void *buffer;      /* Buffer et sa longueur pour les transferts */
	size_t lbuffer;
	
	enum pcmc_pos pos; /* Position logique courante */

	/* En-tete, en format pret a ecrire ou tel que lu */

	struct {
		char param[PCMC_MAXPARAM];  /* Param: {code,valeur} */
	} entete;

	/* Informations sur l'en-tete courant (unite) */

	unsigned long long nbr_total;  /* Nombre total de mots (mots de 8 octets) */
	unsigned long long nbr_donnees;  /* Nombre de mots de donnees (mots de 8 octets) */
	unsigned long long nbr_unused;  /* Nombre de bits inutilises (bits) (64 bit is overkill) */ 
	unsigned int nbr_header;  /* Longueur de l'en-tete (octets) */

	int seek;       /* 0=peut pas, 1=peut, -1=sait pas */
	FILE* fi;       /* Associe a nfi */
	char nfi[200];  /* Nom du fichier */

	int sign;       /* Signature inter-fichier rencontree, 0,1 */
	int debug;      /* Mode debug 0=non, 1=oui */
	int xstdin;     /* input from stdin.  */
	unsigned char md5[16];	

	/* Pointeurs aux fonctions */

	enum pcmc_err (*liberer)       ( struct pcmc_s*);
	enum pcmc_err (*ouvrir)        ( struct pcmc_s*, char*, char*);
	enum pcmc_err (*fileOuvrir)    ( struct pcmc_s*, FILE*, int);
	enum pcmc_err (*fermer)        ( struct pcmc_s*);
	enum pcmc_err (*signLire)      ( struct pcmc_s*, int*);
	enum pcmc_err (*signEcrire)    ( struct pcmc_s*);
	enum pcmc_err (*enteteScan)    ( struct pcmc_s*, long long);
	enum pcmc_err (*enteteLire)    ( struct pcmc_s*);
	enum pcmc_err (*enteteEcrire)  ( struct pcmc_s*, unsigned long long , unsigned long long, char*);
	enum pcmc_err (*donneesCopier) ( struct pcmc_s*, FILE*, unsigned char*);
	enum pcmc_err (*donneesEcrire) ( struct pcmc_s*, FILE*, long long, unsigned char*);
	enum pcmc_err (*donneesEcrire_input_cmcarc) ( struct pcmc_s*, FILE*, long long , MD5_CTX* );
	enum pcmc_err (*donneesSauter) ( struct pcmc_s*);
	enum pcmc_err (*finLire)       ( struct pcmc_s*);
	enum pcmc_err (*finEcrire)     ( struct pcmc_s*, int);
	enum pcmc_err (*signFiEcrire)  ( struct pcmc_s*);
	long          (*paramInfo)     ( struct pcmc_s*, enum pcmc_code, long);
	char*	      (*paramInfoStr)  ( struct pcmc_s*, enum pcmc_code, char*);

} *pcmc, pcmc_a;

#define PCMC_NULL ((pcmc)NULL)

/* En cas d'erreur, pcmcAllouer retourne PCMC_NULL */

enum pcmc_err pcmcAllouer( pcmc *, unsigned int);

#endif
