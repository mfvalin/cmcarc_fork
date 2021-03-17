/*-----------------------------------*/
/* cmcarc.h -- interface du logiciel */
/*-----------------------------------*/

#ifndef CMCARC_H
#define CMCARC_H

#include <sys/types.h>
#include <libgen.h>
#include <unistd.h>
#include "utile.h"
#include "pcmc.h"

/* Nombre de fichiers maximum pour l'ajout,
   nombre de patrons maximum pour l'extraction. */

/* #define CMCARC_MAX_IFI 10000 */

/* Nombre de Ko pour les buffers sur entrees/sorties standard */

#ifndef CMCARC_STDBUFF
#define CMCARC_STDBUFF 8
#endif

/* Nombre de Ko pour les buffers sur disque */

#ifndef CMCARC_DSKBUFF
#define CMCARC_DSKBUFF 256
#endif

/* Informations conservees pour chaque fichier a ajouter ou extraire */

struct cmcarc_fichier {
	char nom[200];     /* Nom du fichier */
	char param[300];   /* Parametres, optionnel */
	int  expn;         /* Numero d'expression reguliere */
	int  mode;         /* Mode (permissions), -99 = pas fait  */
	long long lng;          /* Longueur du fichier en octets */
	long uid;          /* Usager prorpietaire du fichier */
	long gid;          /* Groupe prorpietaire du fichier */
	long mtime;        /* Date de modification du fichier */
	int  valide;       /* 0=non, 1=oui, non=oublier,sauter ce fichier */
	int  match;        /* 0=non, 1=oui, en extraction seulement ('-x') */
	/* char md5[16];	   -* checksum : argument to donneesCopier/Ecrire instead */
};

/* Informations pour l'invocation du logiciel cmcarc */

typedef struct cmcarc_s {
	int lbuff;           /* Nombre de Ko pour les buffers */
	int buffd;           /* Nombre de Ko demande pour les buffers (clef -b) */
	char mode;           /* Ajout, eXtraction, Table ou ' ' */

	char fichier[200];   /* Nom du fichier a traiter ou "" */
	char liste[200];     /* Nom du fichier liste ou "" */
	char prefix[100];    /* Prefixe ou "" */
	char postfix[100];    /* Prefixe ou "" */
	char exec[200];      /* Nom du script d'execution ou "" */

	/* Indicateurs 0=non, 1=oui */

	int debug;   /* Mode debug */
	int help;    /* Afficher version et aide */
	int noclobber; /* Empeche d'overwriter un fichier existant */
	int inform;  /* Mode informatif */
	int inform_detail;  /* Mode informatif encore plus en detail (seulement pour table of content) */
	int mtime_extract; /* On extrait la date de modification des fichiers */
	int match_exit;   /* Si pas de match alors code de sortie <> 0, en extraction seulement ('-x') */
	int path;    /* Utiliser basename seulement dans l'entete */
	int scan;    /* Scan mode; on lit seulement les entetes et on scan pour les signatures */
	long long skip;   /* Nombre de bytes a sauter avant de commencer (utilise pour debugging seulement) */
	int listei;  /* Liste sur stdin (-l sans valeur) */
	int xstdin;  /* Entree sur stdin */
	int xstdout; /* Sortie sur stdout */
	int xstdout_nohdr; /* Sortie sur stdout sans format cmcarc (i.e. le(s) fichier(s) sont envoyes */
	                   /* directement sur stdout                                                   */

	int sequence;  /* --sequence, which file to extract */
	int last;      /* --last, the last file only */
	int dereference;   /* --dereference: follow symlinks */
	int doCRC;	/* --md5: checksum */
	enum version_type version;  /* --32 or --64 */
	int res;     /* Resultat final, EXIT_SUCCESS ou EXIT_FAILURE */

	/* struct cmcarc_fichier fi[CMCARC_MAX_IFI]; */
	struct cmcarc_fichier *fi;
	int fi_alloc;
	int ifi;

	/* Pointeurs aux fonctions */

	int  (*decoder)  ( struct cmcarc_s *, const int, const char **);
	int  (*valider)  ( struct cmcarc_s *);
	int  (*liberer)  ( struct cmcarc_s *);
	int  (*executer) ( struct cmcarc_s *);
	void (*usage)    ( void);

} *cmcarc, cmcarc_a;

#define CMCARC_NULL ((cmcarc)NULL)

/* En cas d'erreur, cmcarcAllouer retourne CMCARC_NULL */

cmcarc cmcarcAllouer(void);

#endif
