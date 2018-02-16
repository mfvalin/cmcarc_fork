/*--------------------------------------*/
/* cmcarc.c -- implantation du logiciel */
/*--------------------------------------*/

#include "cmcarc.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include "use_hash.h"

#ifdef _LARGEFILE64_SOURCE    /* linux */
#define fseek64 fseeko64 
#define ftell64 ftello64
#else
#ifdef AIX
#define fseek64 fseeko64
#define ftell64 ftello64
#endif
#endif

#define CMCARC_VERSION "cmcarc version 4.3.1u [2016/11/22]\n"

static int cmcarchs_size;
/* extern enum version_type cmcarc_version; */

static int  decoder( cmcarc THIS, const int argc, const char **argv);
static int  valider( cmcarc THIS);
static int  liberer( cmcarc THIS);
static int  executer( cmcarc THIS);
static void usage( void);
static int  table( cmcarc THIS, FILE *);
static char *convertPermission( long mode );
static int  ajout( cmcarc THIS);
static int  fichierAjouter(pcmc fica, const char *nom, const char *nn,
                           long long lng, const int mode, const long uid,
                           const long gid, const long mtime, int doCRC);
static int  extraction( cmcarc THIS);
static int  completer( cmcarc THIS);
static int  paramInit( char *buff, unsigned long long *nt, unsigned long long *nd,
                       const char *nom, long long* lng, const int mode,
                       const long uid, const long gid, const long mtime, int doCRC);
static void nomPreparer( char *, const char *, const char *, const char*,const int);


/*----------------------------------------------------------------------------*\
   Nom   : cmcarcAllouer
   But   : Allouer la structure cmcarc et l'initialiser.
   Retour: Retourne le pointeur ou CMCARC_NULL.
\*----------------------------------------------------------------------------*/

cmcarc cmcarcAllouer( void)
{
	cmcarc THIS;

	if( (THIS=(cmcarc)malloc(sizeof(cmcarc_a))) == CMCARC_NULL ) {
		return CMCARC_NULL;
	}

	THIS->decoder  = decoder;
	THIS->valider  = valider;
	THIS->liberer  = liberer;
	THIS->executer = executer;
	THIS->usage    = usage;

#ifdef __sgi
	THIS->fi=(struct cmcarc_fichier*)malloc(sizeof(struct cmcarc_fichier)*5);
	THIS->fi_alloc=5;
#endif
	THIS->res = EXIT_FAILURE;
	return THIS;
}


/*----------------------------------------------------------------------------*\
   Nom   : liberer
   But   : Liberer la memoire utilisee par une structure cmcarc.
   Retour: Le code de sortie prealablement place dans le champs res.
\*----------------------------------------------------------------------------*/

static int liberer( cmcarc THIS)
{
	int r;

	r = THIS->res;

	/* NEW FI */
        free(THIS->fi);

	free(THIS);
	return r;
}


/*----------------------------------------------------------------------------*\
   Nom   : decoder
   But   : Decoder la ligne de commande, resultat dans THIS.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

static int decoder( cmcarc THIS, const int argc, const char **argv)
{
	int i,j;
        char* tmpstr;

	THIS->buffd = -1;         /* On initialise toutes les valeurs */
	THIS->mode  = ' ';

	THIS->fichier[0] = '\0';
	THIS->liste[0]   = '\0';
	THIS->prefix[0]  = '\0';
	THIS->postfix[0]  = '\0';
	THIS->exec[0]    = '\0';

	THIS->debug   = 0;
	THIS->help    = 0;
	THIS->noclobber  = 0;
	THIS->inform  = 0;
	THIS->inform_detail  = 0;
	THIS->mtime_extract  = 0;
	THIS->match_exit  = 0;
	THIS->path    = 0;
	THIS->scan    = 0;
	THIS->skip    = 0;
	THIS->listei  = 0;
	THIS->xstdin  = 0;
	THIS->xstdout = 0;
	THIS->xstdout_nohdr = 0;
	THIS->sequence = 0;
	THIS->last = 0;
	THIS->dereference = 0;
	THIS->doCRC = 0;
	THIS->version = VERSION4;  /* this flag is only used if we try to 
				      concatenate to an existing archive.  Rely
				      on cmcarc_version otherwise to determine version
				      */

        tmpstr=strdup(argv[0]);
        if(strcmp(basename(tmpstr), "cmcarc")!=0) {
           THIS->dereference=1;   /* if it is called ordarc, or whatever, follow symlinks */
        }

	cmcarc_version=VERSION4;   /* default */

	/* NEW FI */
	THIS->fi_alloc = 1000;
	THIS->fi=(struct cmcarc_fichier*)malloc(THIS->fi_alloc * sizeof(struct cmcarc_fichier));


	THIS->ifi = -1;
	i = 1;

	while( i < argc ) {

		if( argv[i][0] != '-' ) {
			fprintf(stderr,
				"\"%s\" is not an option.\n",argv[i]);
			usage();
			return -1;
		}

		switch( argv[i][1] ) {
		case 'f':
			i++;

			if( argv[i] != NULL && argv[i][0] != '-' ) {
				strcpy(THIS->fichier,argv[i]);
			} else {
				fprintf(stderr,
				"You must name the file to manipulate after -f.\n");
				return -1;
			}

			i++;
			break;

		case 'l':
			i++;

			if( argv[i] != NULL && argv[i][0] != '-' ) {
				strcpy(THIS->liste,argv[i]);
				i++;
			} else {

				/* L'option '-l' n'est pas fiable avec ls CFS */

				fprintf(stderr,
					"Option -l without a file name has been disabled.\n");
				return -1;
				/* THIS->listei = 1; */
			}
			break;

		case 'e':
			i++;

			if( argv[i] != NULL && argv[i][0] != '-' ) {
				strcpy(THIS->exec,argv[i]);
				i++;
			} else {
				fprintf(stderr,
				"You must name the execution script after -e.\n");
				return -1;
			}
			break;

		case 'a':
			if( THIS->mode != ' ' ) {
				fprintf(stderr,
				"Select only one action from a,t and x.\n");
				return -1;
			}

			i++;
			THIS->mode = 'a';

			while( argv[i] != NULL && argv[i][0] != '-' ) {

				/* if( THIS->ifi == CMCARC_MAX_IFI-1 ) {  -* not needed, can't add 1000 on cmd line ayway *-
					fprintf(stderr,
						"Too many files after -a, max %d.\n",
						CMCARC_MAX_IFI);
					return -1;
				} */

				j = ++THIS->ifi;

				/* NEW FI */
				if(THIS->ifi==THIS->fi_alloc) {
				  /* grow */
				  struct cmcarc_fichier* temp=(struct cmcarc_fichier*)malloc(THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  memcpy(temp, THIS->fi, THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  free(THIS->fi);
				  THIS->fi=(struct cmcarc_fichier*)malloc(THIS->fi_alloc * 2 * sizeof(struct cmcarc_fichier));
				  memcpy(THIS->fi, temp, THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  THIS->fi_alloc*=2;
                                }

				strcpy(THIS->fi[j].nom,argv[i++]);
				THIS->fi[j].mode = -99;
				THIS->fi[j].valide = 1;
			}

			break;

		case 'x':
			if( THIS->mode != ' ' ) {
				fprintf(stderr,
					"Select only one action from a,t and x.\n");
				return -1;
			}

			i++;
			THIS->mode = 'x';

			while( argv[i] != NULL && argv[i][0] != '-' ) {

				/* if( THIS->ifi == CMCARC_MAX_IFI-1 ) {   -* not needed, can't add 1000 on cmd line ayway *-
					fprintf(stderr,
						"Too many files after -x, max %d.\n",
						CMCARC_MAX_IFI);
					return -1;
				} */

				j = ++THIS->ifi;

				/* NEW FI */
				if(THIS->ifi==THIS->fi_alloc) {
				  /* grow */
				  struct cmcarc_fichier* temp=(struct cmcarc_fichier*)malloc(THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  memcpy(temp, THIS->fi, THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  free(THIS->fi);
				  THIS->fi=(struct cmcarc_fichier*)malloc(THIS->fi_alloc * 2 * sizeof(struct cmcarc_fichier));
				  memcpy(THIS->fi, temp, THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  THIS->fi_alloc*=2;
                                }

				strcpy(THIS->fi[j].nom,argv[i++]);
				THIS->fi[j].mode = -99;
				THIS->fi[j].valide = 1;
			}

			break;

		case 't':
			if( THIS->mode != ' ' ) {
				fprintf(stderr,
					"Select only one action from a,t and x.\n");
				return -1;
			}

			i++;
			THIS->mode = 't';
			break;

		case 'i':
			THIS->xstdin = 1;
			i++;
			break;

		case 'o':
			THIS->xstdout = 1;
			i++;
			break;

		case 'O':
			THIS->xstdout = 1;
			THIS->xstdout_nohdr = 1;
			i++;
			break;

		case 'v':
			THIS->inform = 1;
			i++;
			break;

		case 'V':
			THIS->inform = 1;
			THIS->inform_detail = 1;
			i++;
			break;

		case 'c':
			THIS->noclobber = 1;
			i++;
			break;

		case 'm':
			THIS->mtime_extract = 1;
			i++;
			break;

		case 'M':
			THIS->match_exit = 1;
			i++;
			break;

		case 's':
			THIS->scan = 1;
			i++;
			break;

		case 'S':
			i++;

			if( argv[i] != NULL && argv[i][0] != '-' ) {
			/*	THIS->skip = atol(argv[i]); */
#ifndef AIX
				THIS->skip = atoll(argv[i]);
#else
				THIS->skip = strtoll(argv[i], NULL, 10);
#endif
			} else {
				fprintf(stderr,
					"Give the number of bytes to skip (i.e. starting address) after -b.\n");
				return -1;
			}

			i++;
			break;

		case 'n':
			i++;

			if( argv[i] != NULL && argv[i][0] != '-' ) {
				strcpy(THIS->prefix,argv[i]);
			} else {
				fprintf(stderr,
					"You must give a prefix after -n.\n");
				return -1;
			}

			i++;
			break;

		case 'N':
			i++;

			if( argv[i] != NULL && argv[i][0] != '-' ) {
				strcpy(THIS->postfix,argv[i]);
			} else {
				fprintf(stderr,
					"You must give a postfix after -N.\n");
				return -1;
			}

			i++;
			break;

		case 'b':
			i++;

			if( argv[i] != NULL && argv[i][0] != '-' ) {
				THIS->buffd = atoi(argv[i]);
			} else {
				fprintf(stderr,
					"Give the number of Kb after -b.\n");
				return -1;
			}

			i++;
			break;

		case 'p':
			i++;
			THIS->path = 1;
			break;

		case 'd':
			i++;
			THIS->debug = 1;
			break;

		case 'h':
			THIS->res = 0;
			usage();
			return 1;

		case '-':
			/* --options (we are missing letters) */
			if(strcmp(argv[i], "--sequence")==0) {
			  i++;
			  if(argv[i]!=NULL && atoi(argv[i])>0) {
			    THIS->sequence = atoi(argv[i]);
			    i++;
			    break;
			  } else {
			    fprintf(stderr, "Option --sequence is to be followed by an integer greater then 0.\n");
			    usage();
			    return -1;
			  }
			} else if(strcmp(argv[i], "--last")==0) {
			  THIS->last=1;
			  i++;
			  break;
			} else if(strcmp(argv[i], "--dereference")==0) {
			  THIS->dereference=1;
			  i++;
			  break;
			} else if(strcmp(argv[i], "--md5")==0) {
			  THIS->doCRC=1;
			  i++;
			  break;
			} else if(strcmp(argv[i], "--64")==0) {
			  cmcarc_version=VERSION5;
			  THIS->version=cmcarc_version;
			  i++;
			  break;
			} else if(strcmp(argv[i], "--32")==0) {
			  cmcarc_version=VERSION4;  /* default */
			  THIS->version=cmcarc_version;
			  i++;
			  break;
			} else {
			  fprintf(stderr, "\"%s\" is not an option.\n",argv[i]);
			  usage();
			  return -1;
			}
		default:
			fprintf(stderr,
				"\"%s\" is not an option.\n",argv[i]);
			usage();
			return -1;
		}
	}

#ifdef CFSSHELL
	if( THIS->mode == 'x' && THIS->xstdout != 1) {
		fprintf(stderr, "In this version of cmcarc, option -x has to be followed by -o or -O\n");
		return -1;
	}
#endif
	return 0;
}


/*----------------------------------------------------------------------------*\
   Nom   : valider
   But   : Detecter les combinaisons d'options erronees.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

static int valider( cmcarc THIS)
{
	char msg[500];

	if( THIS->mode == ' ' ) {
		fprintf(stderr,"Choose an action: a,t or x.\n");
		usage();
		return -1;
	}

	if( THIS->xstdout && THIS->mode == 'x' && THIS->exec[0] != '\0' ) {
		fprintf(stderr,"Cannot exec during extract on stdout.\n");
		fprintf(stderr,"(cmcarc format is used in this case)\n");
		return -1;
	}

	if( THIS->xstdin && THIS->listei ) {
		fprintf(stderr,"Conflict on stdin.\n");
		fprintf(stderr,"You cannot use -i and -l at the same time.\n");
		return -1;
	}

	if( THIS->inform && THIS->xstdout ) {
		fprintf(stderr,"Conflict on stdout.\n");
		fprintf(stderr,"You cannot use -v and -o at the same time.\n");
	}

	if( (THIS->mode=='t' || THIS->mode=='x') && !THIS->xstdin ) {
		if( !fichierExiste(THIS->fichier,msg) ) {
			fprintf(stderr,"%s, %s.\n",THIS->fichier,msg);
			return -1;
		}
	}

        if (THIS->scan && THIS->xstdin) {
		fprintf(stderr, "Unable to scan using stdin.\n");
		return -1;
	}


	return 0;
}


/*----------------------------------------------------------------------------*\
   Nom   : executer
   But   : Completer la liste de fichiers (-l) et effectuer le traitement.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

static int executer( cmcarc THIS)
{
	int i,r,tmpFile;
	char n[L_tmpnam];
	char msg[300], comm[300];
	char *nl, *p;

	if( THIS->listei ) {  /* On ne lit pas stdin directement */
		tmpnam(n);
		tmpFile = 1;

		if( stdinCopier(n) != 0 ) {
			fprintf(stderr,"Cannot copy stdin to temporary file.\n");
			return -1;
		}

		nl = n;
	} else {

		/* On peut utiliser le format "machine:chemin" comme avec rcp */

		if( strchr(THIS->liste,':') == NULL ) {
			nl = THIS->liste;
			tmpFile = 0;
		} else {
			tmpnam(n);
			sprintf(comm,"rcp %s %s",THIS->liste,n);

			if( ligneExecuter(comm,msg) != 0 ) {
				fprintf(stderr,"%s\n",msg);
				return -1;
			}

			tmpFile = 1;
			nl = n;
		}
	}

	if ( *nl != '\0' ) {

		/* On peut maintenant lire le fichier associe a la clef '-l' */

		FILE *f;

		if( (f=(FILE*)fopen64(nl,"r")) == NULL ) {
			fprintf(stderr,
				"Cannot open listing file %s, %s.\n",nl,strerror(errno));
			return -1;
		}

		while( fgets(msg,300,f) ) {

			if( (p=strchr(msg,'\n')) == NULL ) {
				fprintf(stderr,
					"Error, no end of line in listing file %s.\n",nl);
				(void)fclose(f);
				return -1;
			}

			*p = '\0';

			i = ++THIS->ifi;

			/* if( i >= CMCARC_MAX_IFI ) {
				fprintf(stderr,"Too many names in listing file %s.\n",nl);
				(void)fclose(f);
				return -1;
			} */

		        /* NEW FI */
			if(THIS->ifi==THIS->fi_alloc) {
				  /* grow */
				  struct cmcarc_fichier* temp=(struct cmcarc_fichier*)malloc(THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  memcpy(temp, THIS->fi, THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  free(THIS->fi);
				  THIS->fi=(struct cmcarc_fichier*)malloc(THIS->fi_alloc * 2 * sizeof(struct cmcarc_fichier));
				  memcpy(THIS->fi, temp, THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  THIS->fi_alloc*=2;
                        }

			if( (p = strchr(msg,' ')) != NULL ) {
				*p++ = '\0';
				strcpy(THIS->fi[i].param,p);
			}

			strcpy(THIS->fi[i].nom,msg);
			THIS->fi[i].mode = -99;
			THIS->fi[i].valide = 1;
		}

		if( fclose(f) != 0 ) {
			fprintf(stderr,"Warning: cannot close listing file %s.\n",nl);
		}
	}

	if( tmpFile ) {
		if( remove(n) != 0 ) {
			fprintf(stderr,
				"Warning: cannot remove temporary file %s.\n",nl);
		}
	}

	switch( THIS->mode ) {
	case 'a':
		if( (r=completer(THIS)) == 0 ) {
			r = ajout(THIS);
		}

		break;

	case 't':
		r = table(THIS,stdout);
		break;

	case 'x':

		for( i=0; i<= THIS->ifi; i++ ) {
			if( (THIS->fi[i].expn = expregCompiler(THIS->fi[i].nom)) == -1 ) {
				break;
			}
		}

		if( THIS->fi[i].expn == -1 ) {
			fprintf(stderr,"Cannot compile regular expression \"%s\".\n",
				THIS->fi[i].nom);
			r = -1;
		} else {
			r = extraction(THIS);
		}

		for( i=0; i<= THIS->ifi; i++ ) {
			expregLiberer(THIS->fi[i].expn);
		}

		break;
	};

	if( r == 0 ) {
		THIS->res = EXIT_SUCCESS;
	}

	return r;
}


/*----------------------------------------------------------------------------*\
   Nom   : usage
   But   : Afficher un resume de la sequence d'appel a cmcarc.
\*----------------------------------------------------------------------------*/

static void usage( void)
{
	static char *aide[] = {
	"\n"
	,"----------------------------------------------------\n"
	,CMCARC_VERSION
#ifdef BETA
	,"\n"
	,"BETA VERSION -- NOT FOR OPERATIONNAL USE\n"
	,"BETA VERSION -- NOT FOR OPERATIONNAL USE\n"
	,"BETA VERSION -- NOT FOR OPERATIONNAL USE\n"
	,"\n"
#endif
	,"----------------------------------------------------\n"
	,"\n"
	,"cmcarc [-a {FILE}] [-b NK_BUFFER] [-c] [-e COMMAND] -f FILE [-h] [-i] [-l [FILE]]\n"
	,"       [-n PREFIX] [-N POSTFIX] [-o] [-O] [-p] [-s] [-t] [-v] [-V] [-x {EXPRESSION}]\n"
	,"	 [--sequence N | --last] [--dereference] [--md5] [--32 | --64]\n\n"
	,"	-a : Add files.\n"
	,"	-b : Number of Kb for transfert buffer.\n"
	,"	-c : Noclobber option (do not overwrite existing file using -x).\n"
	,"	-e : Execution of a command as each file is extracted.\n"
	,"	-f : Name of cmcarc file to modify.\n"
	,"	-h : This help message.\n"
	,"	-i : Read cmcarc file on standard input.\n"
	,"	-l : Complementary list of files for option -a or -x.\n"
	,"	-m : Restore the modification time (default is the current time).\n"
	,"	-M : Return bad exit status if a requested file is not found (with -x only).\n"
	,"	-n : Prefix for names of files added.\n"
	,"	-N : Postfix (suffix) for names of files added.\n"
	,"	-o : Write cmcarc output on stdout.\n"
	,"	-O : Write output file(s) directly on stdout (not imbeded in cmcarc format).\n"
	,"	-p : Combined with -a, write basename of files in headers.\n"
	,"	   : Combined with -x, extract files directly in working directory\n"
	,"	   : (generating a flat directory)\n"
	,"	-s : Scan mode so that every bytes are read using -t.\n"
	,"	-S : Number of bytes to skip (i.e. starting address). Use with care.\n"
	,"	-t : List table of content, on name per line or formatted with -v.\n"
	,"	-v : Informative mode.\n"
	,"	-V : Detailed informative mode (effective when combined with -t option only).\n"
	,"	-x : Extract files using given regular expressions (when available).\n"
	,"	--sequence :    If a file is duplicated in the archive, extract only Nth one\n"
	,"	--last :        If a file is duplicated in the archive, extract only the last one\n"          
	,"	--dereference : Write the file/directory that a symbolic link points to\n"
	,"	                (this is the ordarc default)\n"
	,"	                (cmcarc default is to write the symbolic link)\n"
	,"	--md5 : 	Combined with -a, add md5 (checksum) information to every file.\n"
	,"			Combined with -x, verify that the extracted file has the correct checksum.\n"
	,"	--32 :          Use 32 bit unsigned integers to store file sizes (default)\n"
	,"	--64 :          Use 64 bit unsigned integers to store file sizes (experimental and unsupported)\n"
	};

	int t;

	for( t = 0; t < sizeof(aide)/sizeof(*aide); t++ ) {
		printf("%s",aide[t]);
	}
}

char* hex2string(const unsigned char* str, int size) {
  char* ret=(char*)malloc(2*size+1);  /* 1 u_char = 2 hex char + '\0' */
  int i;

  ret[0]='\0';
  for(i=0; i<size; i++) 
    sprintf(ret, "%s%02x", ret, str[i]);

  ret[2*size]='\0';  /* just to make sure */

  return ret;
}




/*----------------------------------------------------------------------------*\
   Nom   : table
   But   : Afficher sur "out" la table du contenu d'un fichier cmcarc.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

static int table( cmcarc THIS, FILE *out)
{
	pcmc fica;
	enum pcmc_err en;
	long long pos;
	long mode_tmp;
	time_t mtime_tmp;
	int affiche, fini;
	/* int totd,tote,totu,total; */
	long long totd, total;
	int tote, totu;
	char *pmtime;
	char mtime_str[18];

	if( THIS->buffd != -1 ) {
		THIS->lbuff = THIS->buffd;
	} else {
		THIS->lbuff = THIS->xstdin ? CMCARC_STDBUFF : CMCARC_DSKBUFF;
	}

	if( pcmcAllouer(&fica,THIS->lbuff) != PCMCE_OK ) {
		fprintf(stderr,"Not enough memory.\n");
		return -1;
	}
	
	if( THIS->xstdin ) {
		fica->xstdin=1;
		if( fica->fileOuvrir(fica,stdin,0) != PCMCE_OK ) {
			fica->liberer(fica);
			return -1;
		}
	} else {
		if( fica->ouvrir(fica,THIS->fichier,"rb") != PCMCE_OK ) {
			fica->liberer(fica);
			return -1;
		}
	}

	if( fica->signLire(fica,&cmcarchs_size) != PCMCE_OK ) {
		fica->fermer(fica);
		fica->liberer(fica);
		return -1;
	}

	if ( THIS->skip ) {
		if( fica->seek && (pos=ftell64(fica->fi)) == -1 ) {
			fprintf(stderr,
				"Warning: Cannot get current position in file %s, %s.\n",
				fica->nfi,strerror(errno));
		}
		fprintf(stderr, "Skipping first %lld bytes ...\n", THIS->skip);
		if( fseek64(fica->fi,THIS->skip,SEEK_SET) == -1 ) {
			return PCMCE_SEEK;
		}
	}


	total = totd = tote = totu = 0;
	fini = 0;

	if( THIS->scan ) {
		fprintf(out,"[Scan mode]\n");
		fprintf(out,"%-14s %-17s\n","Address","Filename");
		fprintf(out,"%-14s %-17s\n",
			"--------------",
			"-----------------");
	} else {
		if( THIS->inform ) {

			if ( THIS->inform_detail ) {
				fprintf(out,"%-6s %4s %4s %17s %12s %6s %6s %32s %-17s\n",
				"Mode","uid","gid","Time","Data","Header","UBC","MD5 checksum", "Filename");
				fprintf(out,"%-6s %4s %4s %17s %12s %6s %6s %32s %-17s\n",
					"------",
					"----",
					"----",
					"-----------------",
					"------------",
					"------",
					"------",
					"--------------------------------", 
					"-----------------"); 
			} else {
				fprintf(out,"%-10s %4s %4s %10s %17s %-28s\n",
				"Mode","uid","gid","Size","Time","Filename");
				fprintf(out,"%-10s %4s %4s %10s %17s %-28s\n",
					"----------",
					"----",
					"----",
					"----------",
					"-----------------",
					"----------------------------");
			}
		}
	}

	if( fica->seek && (pos=ftell64(fica->fi)) == -1 ) {
		fprintf(stderr,
			"Warning: Cannot get current position in file %s, %s.\n",
			fica->nfi,strerror(errno));
	}

	do {  /* On passe au-travers de tous les fichiers un par un */

		while( (en=fica->enteteLire(fica)) == PCMCE_OK ) {

			if( !THIS->scan) {

				if( fica->donneesSauter(fica) != PCMCE_OK ) {
				 	fprintf(stderr,
		     			"ERROR: error in %s\n",fica->entete.param+1);
       				         fica->fermer(fica);
       				         fica->liberer(fica);
					return PCMCE_ERR;
				}

				/* On n'affiche pas les fichiers bidon inter-fichier, ni une signature */
				/* cmcarc qui pourrait se retrouver dans le milieu dans la cas d'une   */
				/* concatenation de 2 fichiers par exemple.                            */
				affiche = strcmp(fica->entete.param,PCMC_SIGN_IFI_CMCARC);
				if ( affiche ) {
					/* affiche = strcmp(fica->entete.param,PCMC_SIGN_CMCARC); */
				    if(strcmp(fica->entete.param,PCMC_SIGN_CMCARC)==0 ||
					strcmp(fica->entete.param,PCMC_SIGN_CMCARC_OLD4)==0)
					   affiche=0;   /* signature, don't display */		
				}
	
				if( THIS->inform ) {
	
#ifdef _SX
					mtime_tmp = (time_t *)fica->paramInfo(fica,PCMCC_MTIME,0);
#else
					mtime_tmp = fica->paramInfo(fica,PCMCC_MTIME,0);
#endif
					pmtime = ctime(&mtime_tmp);
					sprintf(mtime_str,"%.12s %.4s\0",&pmtime[4],&pmtime[20]);

					if( THIS->inform_detail ) {
						if( affiche ) {
							fprintf(out,"%6lo %4lu %4lu %17s %12llu %6llu %6llu %32s %-s",
								fica->paramInfo(fica,PCMCC_MODE,0755),
								fica->paramInfo(fica,PCMCC_UID,0),
								fica->paramInfo(fica,PCMCC_GID,0),
								mtime_str, fica->nbr_donnees,
								(unsigned long long) fica->nbr_total - fica->nbr_donnees-2, 
								fica->nbr_unused,
								hex2string(fica->md5, 16), 
								fica->entete.param+1);
							if(S_ISLNK(fica->paramInfo(fica,PCMCC_MODE,0755)))
								fprintf(out," -> %s", fica->paramInfoStr(fica, PCMCC_LINK, ""));
							fprintf(out, "\n");
						}
	
						totd  += fica->nbr_donnees;
						tote  += fica->nbr_total - fica->nbr_donnees;
						totu  += fica->nbr_unused;
						total += fica->nbr_total;
					} else {
						if( affiche ) {
							mode_tmp = fica->paramInfo(fica,PCMCC_MODE,0755);
							fprintf(out,"%10s %4lu %4lu %10llu %17s %-s",
								convertPermission(mode_tmp),
								fica->paramInfo(fica,PCMCC_UID,0),
								fica->paramInfo(fica,PCMCC_GID,0),
								(unsigned long long) fica->nbr_donnees*8-fica->nbr_unused/8,
								mtime_str, fica->entete.param+1);
							if(S_ISLNK(mode_tmp)) 
								fprintf(out," -> %s", fica->paramInfoStr(fica, PCMCC_LINK, ""));
							fprintf(out,"\n");
						}
					}
				} else {
	
					if( affiche ) {

						if( THIS->xstdout ) {
							fprintf(out,"%s %llu\n",
								fica->entete.param+1,
								(unsigned long long)fica->nbr_donnees*8-fica->nbr_unused/8);
							fflush(out);
						} else {
							fprintf(out,"%s\n",fica->entete.param+1);
							fflush(out);
						}
					} else {
						fica->sign = 1;
					}
				}

			} else {  /* On scan le fichier seulement */

				/* On n'affiche pas les fichiers bidon inter-fichier, ni une signature */
				/* cmcarc qui pourrait se retrouver dans le milieu dans la cas d'une   */
				/* concatenation de 2 fichiers par exemple.                            */
				affiche = strcmp(fica->entete.param,PCMC_SIGN_IFI_CMCARC);
				if ( affiche ) {
					/* affiche = strcmp(fica->entete.param,PCMC_SIGN_CMCARC); */
				    if(strcmp(fica->entete.param,PCMC_SIGN_CMCARC)==0 ||
					strcmp(fica->entete.param,PCMC_SIGN_CMCARC_OLD4)==0)
					   affiche=0;  /* signature, don't display */
				}

				if( affiche ) {
					fprintf(out,"%-14lld %s\n", pos, fica->entete.param+1);
					fflush(out);
				}
				if( fica->enteteScan(fica,pos) != PCMCE_OK ) {
					fica->fermer(fica);
					fica->liberer(fica);
					return -1;
				}

				fica->pos = PCMCP_ENTETE;

				if( fica->seek && (pos=ftell64(fica->fi)) == -1 ) {
					fprintf(stderr,
						"Warning: Cannot get current position in file %s, %s.\n",
						fica->nfi,strerror(errno));
				}
			} /* if !scan */
		}

		if( en == PCMCE_EOF ) {
			fini = 1;
		}
	} while( !fini );

	if( THIS->inform ) {
		if( THIS->inform_detail ) {
			fprintf(out,"%6s %4s %4s %17s %12s %6s %6s %32s %-17s\n",
			"------",
			"----",
			"----",
			"-----------------",
			"------------",
			"------",
			"------",
		        "--------------------------------", 
			"-----------------");
			fprintf(out,"%6s %4s %4s %17s %12s %6s %6s %-17s\n",
			" "," "," "," ","Data","Header","UBC"," ");
			fprintf(out,"%6s %4s %4s %17s %12s %6s %6s %-17s\n",
			" ",
			" ",
			" ",
			" ",
			"------------",
			"------",
			"------",
			" ");
			fprintf(out,"%34.34s %12lld %6d %6d\n",
			"Total:",totd,tote,totu);
			fprintf(out,"%34.34s %12lld %6d %6d\n",
			"In bytes:",totd*8,tote*8,totu>>3);
			fprintf(out,"File total = %lld bytes.\n",cmcarchs_size+8*total);
		}
	}

	fica->fermer(fica);
	fica->liberer(fica);
	return 0;
}


/*----------------------------------------------------------------------------*\
   Nom   : convertPermission
   But   : Convertir le champ mode en format -rwxrwxrwx
   Retour: La chaine de caractere -rwxrwxrwx
Modif: to support softlinks
\*----------------------------------------------------------------------------*/

static char *convertPermission( long mode )
{

	static char perm[11];
	char *access[6];
	int usr_type[3];
	int j,n=0;

	usr_type[0] = S_IRUSR;  /* read permission: owner    */
	usr_type[1] = S_IWUSR;  /* write permission: owner   */
	usr_type[2] = S_IXUSR;  /* execute permission: owner */

	/* done for same reason as above */
	access[0] = "r";    access[1] = "w";    access[2] = "x";
	access[3] = "s";    access[4] = "t";    access[5] = "l";

        /* check what type of file (only dir or standard file) */
        if (S_ISDIR(mode))
                strcpy(perm, "d");
        else if(S_ISLNK(mode))
		strcpy(perm, "l");
	else
                strcpy(perm, "-");

        /* identify permission bits */
        for(n=0; n<=2; n++)
                for(j=0; j<=2; j++)
                {
                        if ((n == 0 && j == 2) && (mode & S_ISUID))
                        strcat(perm, access[3]);
                        else if ((n == 1 && j == 2) && (mode & S_ISGID) && (mode & 010))
                        strcat(perm, access[3]);
                        else if ((n == 1 && j == 2) && (mode & S_ISGID))
                        strcat(perm, access[5]);
                        else if ((n == 2 && j == 2) && (mode & S_ISVTX))
                        strcat(perm, access[4]);
                        else if(mode & (usr_type[j] >> (n*3)))
                        strcat(perm, access[j]);
                else
                        strcat(perm, "-");
                }
        return perm;
}




/*----------------------------------------------------------------------------*\
   Nom   : ajout
   But   : Ajouter des fichiers a un fichier cmcarc sur disque ou sur stdout.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

static int ajout( cmcarc THIS)
{
	int i,existe;
	pcmc fica;
	char nom[300];
	enum pcmc_err perr;
	
	if( THIS->buffd != -1 ) {
		THIS->lbuff = THIS->buffd;
	} else {
		THIS->lbuff = THIS->xstdout ? CMCARC_STDBUFF : CMCARC_DSKBUFF;
	}

	if( pcmcAllouer(&fica,THIS->lbuff) != PCMCE_OK ) {
		fprintf(stderr,"Not enough memory.\n");
		return -1;
	}

	if(THIS->xstdin) {
	  fica->xstdin=1;
	}

	if( THIS->xstdout ) {
		if( fica->fileOuvrir(fica,stdout,0) != PCMCE_OK ) {
			fica->liberer(fica);
			return -1;
		}

		if( fica->signEcrire(fica) != PCMCE_OK ) {
			fica->liberer(fica);
			return -1;
		}
	} else {
		existe = fichierExiste(THIS->fichier,NULL);

		if( existe ) {
			if( fica->ouvrir(fica,THIS->fichier,"rb+") != PCMCE_OK ) {
				fica->liberer(fica);
				return -1;
			}

			perr = fica->signLire(fica,&cmcarchs_size);

			if( ( perr != PCMCE_OK ) && ( perr != PCMCE_EOF ) ) {
				fica->fermer(fica);
				fica->liberer(fica);
				return -1;
			}
			

		} else {
			if( fica->ouvrir(fica,THIS->fichier,"wb") != PCMCE_OK ) {
				fica->liberer(fica);
				return -1;
			}
		}

		if( fseek64(fica->fi,0,SEEK_END) != 0 ) {
			fica->fermer(fica);
			fica->liberer(fica);
			return -1;
		}


		if( ftell64(fica->fi) == 0 ) {
			if( fica->signEcrire(fica) != PCMCE_OK ) {
				fica->liberer(fica);
				return -1;
			}
		}

		
		/* careful, if the existing cmcarc file is version 4, we want to 
		  concatenate a v5 header to it */
		if(existe && cmcarc_version!=THIS->version) {
			/* we read a vers4 archive, and we want to concatenante 
			 * a vers5 archive to it, or vice versa */
			cmcarc_version=THIS->version;
			fica->pos = PCMCP_SIGN;
			if( fica->signEcrire(fica) != PCMCE_OK ) {
                            fica->liberer(fica);
                            return -1;
                	}
		}
		
		fica->pos = PCMCP_ENTETE;
	}

	for( i=0; i<=THIS->ifi; i++ ) {

		/* Les fichiers non-reguliers sont sautes ( valide != 1 ) */

		if( THIS->fi[i].valide == 1 ) {
			nomPreparer(nom,THIS->fi[i].nom,THIS->prefix,THIS->postfix,THIS->path);

			if( fichierAjouter(fica,
				THIS->fi[i].nom,nom,THIS->fi[i].lng,THIS->fi[i].mode,
				THIS->fi[i].uid,THIS->fi[i].gid,THIS->fi[i].mtime, THIS->doCRC) != 0 )
			{
				fica->fermer(fica);
				fica->liberer(fica);
				fprintf(stderr, "Cannot add file %s.\n", THIS->fi[i].nom);
				return -1;
			} else {
				if( THIS->inform ) {
					printf("ADDED: %s\n",THIS->fi[i].nom);
				}
			}
		}
	}

	fica->fermer(fica);
	fica->liberer(fica);
	return 0;
}


/*----------------------------------------------------------------------------*\
   Nom   : fichierAjouter
   But   : Ajouter une signature suivie d'UN fichier a un fichier cmcarc.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

static int fichierAjouter( pcmc fica, const char *nom, const char *nn,
                           long long lng, const int mode, const long uid,
                           const long gid, const long mtime, int doCRC)
{
	FILE *f;
	/* int nt,nd,nu; */
	unsigned long long nt, nd;
	int nu;  /* original */
	unsigned char* md5=NULL;

	char buff[2000];

	if(doCRC)
	  md5=fica->md5;

	if( fica->signFiEcrire(fica) != PCMCE_OK ) {
		return -1;
	}


	if( paramInit( buff, &nt, &nd, nn, &lng, mode, uid, gid, mtime, doCRC) != 0 ) {
		return -1;
	}

	if( !S_ISLNK(mode) && (f=(FILE*)fopen64(nom,"rb")) == NULL ) {
		fprintf(stderr,
			"Cannot open file %s for reading, %s.\n",nom,strerror(errno));
		return -1;
	}

	if( fica->enteteEcrire(fica,nt,nd,buff) != PCMCE_OK ) {
		(void)fclose(f);
		return -1;
	}

	if( lng == 0 ) {
		fica->pos = PCMCP_FIN;  /* Pas d'operations inutiles */
	} else {
	        if( fica->donneesEcrire(fica,f,lng, md5) != PCMCE_OK ) {
			(void)fclose(f);
			return -1;
		}
	}

	if( !S_ISLNK(mode) && fclose(f) != 0 ) {
		fprintf(stderr,"Cannot close file %s after reading.\n",nom);
		return -1;
	}

	nu = (long long) (lng % 8) ? (8-(lng % 8))*8 : 0;
	if(doCRC && fica->nbr_donnees)
	  /* write cksum for non-empty files only */
	  nu+=128;  /* in bits!!! */

	if( fica->finEcrire(fica,nu) != PCMCE_OK ) {
		return -1;
	}

	return 0;
}


/*----------------------------------------------------------------------------*\
   Nom   : completer
   But   : Completer la liste des fichiers a ajouter en lisant les repertoires.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

static int completer( cmcarc THIS)
{
	int r,i,imax;
	char msg[500];
	/* char *ptr[CMCARC_MAX_IFI];
	static char noms[500000];  */
	char **ptr;

	imax = THIS->ifi;

	/* Lire la longueur et le mode des fichiers, lire les sous-repertoires */

	for( i=0; i <= imax; i++ ) {

		r = fichierInfo(THIS->fi[i].nom,
		    &THIS->fi[i].lng,&THIS->fi[i].mode,&THIS->fi[i].uid,
		    &THIS->fi[i].gid,&THIS->fi[i].mtime,msg, 
		    THIS->dereference);

		if( r != 0 ) {
			fprintf(stderr,
				"Cannot get informations about file %s, %s. #1\n",
				THIS->fi[i].nom,msg);
			return -1;
		}

		if( modeRepertoire(THIS->fi[i].mode) ) {
			int j,k;

			THIS->fi[i].lng = 0;
			/* r = repertoireLister(THIS->fi[i].nom,ptr,noms,CMCARC_MAX_IFI,500000,msg); */
			r = repertoireLister(THIS->fi[i].nom, &ptr, msg);

			if( r < 0 ) {
				fprintf(stderr,"Cannot list directory %s, %s.\n",
					THIS->fi[i].nom,msg);
				return -1;
			}

			for( j=0; j < r; j++ ) {
				k = ++THIS->ifi;

				/* if( k >= CMCARC_MAX_IFI-1 ) {
					fprintf(stderr,
						"File buffer overflow (more than %d files).\n",
						CMCARC_MAX_IFI);
					return -1;
				} */

				/* NEW FI */
				if(k==THIS->fi_alloc) {
				  /* grow */
				  struct cmcarc_fichier* temp=(struct cmcarc_fichier*)malloc(THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  memcpy(temp, THIS->fi, THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  free(THIS->fi);
				  THIS->fi=(struct cmcarc_fichier*)malloc(THIS->fi_alloc * 2 * sizeof(struct cmcarc_fichier));
				  memcpy(THIS->fi, temp, THIS->fi_alloc * sizeof(struct cmcarc_fichier));
				  THIS->fi_alloc*=2;
                                }

				strcpy(THIS->fi[k].nom,ptr[j]);
				free(ptr[j]);
				THIS->fi[k].mode = -99;
				THIS->fi[k].valide = 1;
			}
		} else if ( !modeRegulier(THIS->fi[i].mode) ) {
			THIS->fi[i].valide = 0;
			fprintf(stderr,
				"%s skipped because it is not a regular file.\n",
				THIS->fi[i].nom);
		}
	}

	/* free(ptr);   -* core dumps, why? */

	/* Lire la longueur et le mode de chaque element ajoute a la liste */

	for( i=imax+1; i <= THIS->ifi; i++ ) {

		r = fichierInfo(THIS->fi[i].nom,
		    &THIS->fi[i].lng,&THIS->fi[i].mode,&THIS->fi[i].uid,
		    &THIS->fi[i].gid,&THIS->fi[i].mtime,msg, 
		    THIS->dereference);

		if( r != 0 ) {
			fprintf(stderr,"Cannot get informations about file %s, %s %d#2.\n",
				THIS->fi[i].nom,msg, i);
			return -1;
		}
	}

	return 0;
}


#define MAX_BU 5000

/*----------------------------------------------------------------------------*\
   Nom   : paramInit
   But   : Preparer les informations a placer dans un entete de fichier cmcarc.
   Retour: 0=Ok, -1=Erreur.
nt: total words, nd: total data words (OUT)
lng: total bytes (IN/OUT) (reset to 0 in case of symlink) 
\*----------------------------------------------------------------------------*/

static int paramInit( char *buff, unsigned long long *nt, unsigned long long *nd,
                      const char *nom, long long* lng, const int mode,
                      const long uid, const long gid, const long mtime, int doCRC)
{
	char bu[MAX_BU];
	char cmode[MAX_BU], cuid[MAX_BU],cgid[MAX_BU],cmtime[MAX_BU];
	char *p;
	int lp,np;
	int i,j;
	char* target;    /* for symlinks */

	p = bu + MAX_BU - 1;

	while( p > bu ) { /* Les positions non utilises sont initialisees a NUL */
		*p-- = '\0';
	}

	for ( i = sizeof(mode) - 1, j = 0; i >= 0; i--) {
		cmode[j] = (mode >> (i * 8)) & 255;
		if( cmode[j] != '\0' ) {
			cmode[++j] = '\0'; /* On s'assure que la chaine de caractere termine par \0 */
		} else if((cmode[j] == '\0') && (j > 0)) {
			cmode[j] = '\001'; /* On ne peut avoir un caractere null dans la chaine */
			cmode[++j] = '\0'; /* On s'assure que la chaine de caractere termine par \0 */
		}
	}

	for ( i = sizeof(uid) - 1, j = 0; i >= 0; i--) {
		cuid[j] = (uid >> (i * 8)) & 255;
		if( cuid[j] != '\0' ) {
			cuid[++j] = '\0'; /* On s'assure que la chaine de caractere termine par \0 */
		} else if((cuid[j] == '\0') && (j > 0)) {
			cuid[j] = '\001'; /* On ne peut avoir un caractere null dans la chaine */
			cuid[++j] = '\0'; /* On s'assure que la chaine de caractere termine par \0 */
		}
	}

	for ( i = sizeof(gid) - 1, j = 0; i >= 0; i--) {
		cgid[j] = (gid >> (i * 8)) & 255;
		if( cgid[j] != '\0' ) {
			cgid[++j] = '\0'; /* On s'assure que la chaine de caractere termine par \0 */
		} else if((cgid[j] == '\0') && (j > 0)) {
			cgid[j] = '\001'; /* On ne peut avoir un caractere null dans la chaine */
			cgid[++j] = '\0'; /* On s'assure que la chaine de caractere termine par \0 */
		}
	}

	for ( i = sizeof(mtime) - 1, j = 0; i >= 0; i--) {
		cmtime[j] = (mtime >> (i * 8)) & 255;
		if( cmtime[j] != '\0' ) { 
			cmtime[++j] = '\0'; /* On s'assure que la chaine de caractere termine par \0 */
		} /* else if((cmtime[j] == '\0') && (j > 0)) {  ! this is potentially very dangerous ! 
			cmtime[j] = '\001'; -* On ne peut avoir un caractere null dans la chaine *-
			cmtime[++j] = '\0'; -* On s'assure que la chaine de caractere termine par \0 *-
		} */
	}

	lp = (1 + strlen(nom) + 1) + (1 + strlen(cmode) + 1) + 
	     (1 + strlen(cuid) + 1) + (1 + strlen(cgid) + 1) + 
	     (1 + strlen(cmtime) + 1);  
	     /* (1 + 4 +1) */;
	     

	/* if symlink, add target */
	if(S_ISLNK(mode)) {
	  int target_size;
	  target=malloc(*lng + 1);
	  if((target_size=readlink(nom, target, (size_t)(*lng)))<0) {

	     fprintf(stderr, "Can't read link %s: %d\n", nom, errno);
	     return -1;
	  }

          target[target_size]='\0';   /* in sgi, target is not NULL terminated */

	  if(target_size != *lng) {
	    fprintf(stderr, "Error link %s lng=%lld size=%d\n", nom, *lng, target_size);
	    return -1;
	  }
	  lp+=(2 + target_size);
	  *lng=0;   /* for symlinks, reset size to 0 */
        } else if(S_ISDIR(mode)) {    /* AF */
          *lng=0;  /* dir, size is 0??? */
        }

	if( lp % 8 ) {
		lp += 8 - (lp % 8);  /* Ajuster sur un multiple de 64 bits */
	}

	if( lp >= MAX_BU ) {
		fprintf(stderr,"Buffer overflow (internal error in paramInit).\n");
		return -1;
	}

	if(S_ISLNK(mode)) 
	  sprintf(bu,"%c%s%c%c%s%c%c%s%c%c%s%c%c%s%c%c%s%c",PCMCC_NOMF,nom,'\0',
		PCMCC_MODE,cmode,'\0',PCMCC_UID,cuid,'\0',
		PCMCC_GID,cgid,'\0',PCMCC_MTIME,cmtime,'\0', PCMCC_LINK, target, '\0');
	else 
	  sprintf(bu,"%c%s%c%c%s%c%c%s%c%c%s%c%c%s%c",PCMCC_NOMF,nom,'\0',
		PCMCC_MODE,cmode,'\0',PCMCC_UID,cuid,'\0',
		PCMCC_GID,cgid,'\0',PCMCC_MTIME,cmtime,'\0');  /* cmtime is still dangerous for \0 */
	  /* sprintf(bu,"%c%s%c%c%s%c%c%s%c%c%s%c%c%c%c%c%c%c",PCMCC_NOMF,nom,'\0',
		PCMCC_MODE,cmode,'\0',PCMCC_UID,cuid,'\0',
		PCMCC_GID,cgid,'\0',PCMCC_MTIME,cmtime[0], cmtime[1], cmtime[2], cmtime[3],'\0'); */

	  
	np = lp / 8;
	*nd = (long long) *lng % 8 ? *lng/8+1 : *lng/8;
	if(doCRC && *nd) 
	  /* record cksum only for non-empty files */
	  *nd += 2;   /* md5 = 16 bytes = 2 words */

	if(cmcarc_version==VERSION5) {
	  *nt = *nd + np + 4;  /* 64 bits */
	} else {
	  *nt = *nd + np + 2; 
	}

	p = bu;

	while( lp-- > 0 ) {
		*buff++ = *p++;
	}

	return 0;
}


/* extract a single element (dir or file) */

int extract_this(cmcarc THIS, pcmc fica, pcmc ficao, int file_pos) {
  char* nom, *nom_de_base;
  char msg[1000];
  int mode;
  int res=0;
  unsigned char* md5=NULL;

  if(THIS->doCRC)
	  md5=ficao->md5;

  nom = fica->entete.param+1;
  
  mode = fica->paramInfo(fica,PCMCC_MODE,0755);
  if( !THIS->xstdout && (modeRepertoire(mode) || S_ISLNK(mode))) { 
    if( modeRepertoire(mode) && repertoireCreer(nom,mode,msg) != 0 ) {
      fprintf(stderr,
	      "Warning: %s, trying to scan next header.\n",msg);
    } 
    else if(S_ISLNK(mode)) {
      char* source = fica->paramInfoStr(fica, PCMCC_LINK, "");
      if( cheminCreer(nom,msg) != 0 ) {
	fprintf(stderr,"Warning: %s\n",msg);
	res = 2;
      } else if(symlink(source, nom) != 0) {
	 if(errno==EEXIST) {
	   if(THIS->noclobber) {
	     fprintf(stderr,
  		"Warning: %s: file already exists and noclobbber option set.\n",nom);
	     res = 2;
	   } else {
	     /* we should attempt to unlink the link */
	     unlink(nom);
	     if(symlink(source, nom) != 0)
               fprintf(stderr, "Warning: can't create symbolic link %s, trying to scan next header.\n",source);
	   }
	 } else
           fprintf(stderr, "Warning: can't create symbolic link %s, trying to scan next header.\n",source);
      } else if(THIS->inform)
        printf("Linking %s\n", nom);
    }

    /* just to read ending */
    if( fica->donneesSauter(fica) != PCMCE_OK ) {
      fprintf(stderr,
		"ERROR: error in %s\n",fica->entete.param+1);
      fica->fermer(fica);
      ficao->fermer(ficao);
      fica->liberer(fica);
      ficao->liberer(ficao);
      return PCMCE_ERR;
    }

  } else {
    if( THIS->xstdout ) {
      int ok;
      
      ok = 1;
      
      if( !THIS->xstdout_nohdr ) {
	if( ficao->signFiEcrire(ficao) != PCMCE_OK ) {
	  ok = 0;
	}
      }
      
      if( !THIS->xstdout_nohdr ) {
	/* carefull, nbr_total can change if transferring 
	   from v4 to v5 */
	unsigned long long new_nbr_total=fica->nbr_total;
	if(cmcarc_version==VERSION4)
	  /* new_nbr_total+=2;  -* total size increases */
	  new_nbr_total+=4;  /* total size increases, including checksum */
	if( ok && ficao->enteteEcrire(ficao,new_nbr_total,fica->nbr_donnees,
				      fica->entete.param) != PCMCE_OK )
	  {
	    ok = 0;
	  }
      }
      
      if(THIS->xstdout_nohdr) {
	ficao->pos = PCMCP_DONNEES;
	if( ok && ficao->donneesCopier(fica,ficao->fi, md5)!=PCMCE_OK )
	  {
	    ok = 0;
	  }
	if(THIS->doCRC && ok && memcmp(fica->md5, md5, 16)!=0)
	  {
	    /* checksum d.n. match */
	    if(memcmp(fica->md5, MD5_NULL, 16)==0) {
	      /* 'cause the input file didn't have a checksum */
	      fprintf(stderr,"Warning: File %s was archived without checksum information.\n", nom);
	    } else {
		fica->fermer(fica);
		ficao->fermer(ficao);
		fica->liberer(fica);
		ficao->liberer(ficao);
	        fprintf(stderr,"Error: File %s checksum doesn't match, aborting.\n", nom);
		return -1;
	    }
	  }
      } else {
	  MD5_CTX* md5_ctx;
	  char last_word[8];
	  if(THIS->doCRC  && fica->nbr_donnees) {
	    md5_ctx=malloc(sizeof(MD5_CTX));
	    MD5_Init(md5_ctx);
	    /* if we require checksum, we will read the last word separately */
	  } else {
	    md5_ctx=NULL;
	  }

	  if( ok ) {
	    if(THIS->doCRC  && fica->nbr_donnees) {
	       if(ficao->donneesEcrire_input_cmcarc(ficao,fica->fi,(long long)(fica->nbr_donnees-1)*8, md5_ctx)!=PCMCE_OK ) {
	          ok = 0;
		}
	    } else {
		if(ficao->donneesEcrire(ficao,fica->fi,(long long)fica->nbr_donnees*8, NULL)!=PCMCE_OK ) {
		  ok = 0;
		}
	    }
	  }
	
	  if(THIS->doCRC && fica->nbr_donnees) {
	    /* read & write last word */
	    if( fread(last_word,1,8,fica->fi) != 8 ) {
	       fprintf(stderr, "Cannot read data just before unused bits, %s.\n",
		strerror(errno));
	        return PCMCE_I;
	    }
	    if( fwrite((void *)last_word,1,8,ficao->fi) != 8 ) {
	       fprintf(stderr, "Cannot write last word of data, %s.\n",
		strerror(errno));
	        return PCMCE_I;
	    }
	  }

	  fica->pos = PCMCP_FIN;
	  
	  if( ok && fica->finLire(fica) != PCMCE_OK ) {
	    ok = 0;
	  } 

	  if(THIS->doCRC  && fica->nbr_donnees && ok) {
	    MD5_Update(md5_ctx, last_word, 8-fica->nbr_unused/8);
	    MD5_Final(md5, md5_ctx);

	    if(memcmp(fica->md5, md5, 16)!=0) {
	      /* checksum d.n. match */
	      if(memcmp(fica->md5, MD5_NULL, 16)==0) {
	        /* 'cause the input file didn't have a checksum */
	        fprintf(stderr,"Warning: File %s was archived without checksum information.\n", nom);
	      } else {
		fica->fermer(fica);
		ficao->fermer(ficao);
		fica->liberer(fica);
		ficao->liberer(ficao);
	        fprintf(stderr,"Error: File %s checksum doesn't match, aborting.\n", nom);
		return -1;
	      }
	    } 
	  }

	  /* no matter what, copy over input file's checksum info */
	  memcpy(ficao->md5, fica->md5, 16);

	  if( ok && ficao->finEcrire(ficao,fica->nbr_unused) != PCMCE_OK ) {
	    ok = 0;
	  }
	}
      
      if( !ok ) {
	fica->fermer(fica);
	ficao->fermer(ficao);
	fica->liberer(fica);
	ficao->liberer(ficao);
	fprintf(stderr,"Error while extracting file %s.\n",nom);
	return -1;
      } else {
	if( THIS->inform ) {
	  printf("EXTRACTED: %s\n",nom);
	}
      }
    } else {
      
      /* Extraction sur disque et traitement par exec */
      
      FILE *f;
      char exec_buffer[600];
      
      /* On genere un flat directory */
      if( THIS->path ) {
	if( (nom_de_base = strrchr(nom,'/')) != NULL ) {
	  nom = ++nom_de_base;
	}
      }
      
      if( cheminCreer(nom,msg) != 0 ) {
	fprintf(stderr,"Warning: %s\n",msg);
	fica->donneesSauter(fica);
	res = 2;
	return 2;
      }
      
      if( THIS->noclobber && fichierExiste(nom,NULL) ) {
	fprintf(stderr,
		"Warning: %s: file already exists and noclobbber option set.\n",nom);
	fica->donneesSauter(fica);
	res = 2;
	return 2;
      }
      
      if( (f=(FILE*)fopen64(nom,"wb")) == NULL ) {
	fprintf(stderr,
		"Warning: Cannot open file %s for writing, %s.\n",
		nom,strerror(errno));
	fica->donneesSauter(fica);
	res = 2;
	return 2;
      }
      
      if( fica->donneesCopier(fica,f, md5) != PCMCE_OK ) {
	fprintf(stderr,
		"ERROR: Cannot extract data in %s\n"
		,fica->entete.param+1);
	fica->fermer(fica);
	fica->liberer(fica);
	return PCMCE_ERR;
      }
      
      if(THIS->doCRC && memcmp(fica->md5, md5, 16)!=0)
	 {
	    /* checksum d.n. match */
	    if(memcmp(fica->md5, MD5_NULL, 16)==0) {
	      /* 'cause the input file didn't have a checksum */
	      fprintf(stderr,"Warning: File %s was archived without checksum information.\n", nom);
	    } else {
		fica->fermer(fica);
		fica->liberer(fica);
	        fprintf(stderr,"Error: File %s checksum doesn't match, aborting.\n", nom);
		return -1;
	    }
	  }

      if( fclose(f) != 0 ) {
	fprintf(stderr,"Warning: Cannot close file %s.\n",nom);
      }
      
      if( fichierModeSet(nom,fica->paramInfo(fica,PCMCC_MODE,0755),msg) != 0 ) {
	fprintf(stderr,"Warning: %s, %s.\n",nom,msg);
	res = 1;
      }
      
      if( THIS->mtime_extract ) {
	if( fichierTimeSet(nom,fica->paramInfo(fica,PCMCC_MTIME,0),msg) != 0 ) {
	  fprintf(stderr,"Warning: %s, %s.\n",nom,msg);
	  res = 1;
	}
      }
      
      if( THIS->inform ) {
	printf("EXTRACTED: %s\n",nom);
      }
      
      if( THIS->exec[0] != '\0' ) {
	
/*
	if( THIS->fi[i].param[0] != '\0' ) {
	  sprintf(exec_buffer,"%s %s %s",
		  THIS->exec,nom,THIS->fi[i].param);
 I believe(d) this to be an old bug - was I wrong? AF  */

        /* this is the only use of parameter file_pos ... */

	if( THIS->fi[file_pos].param[0] != '\0' ) {
	  sprintf(exec_buffer,"%s %s %s",
		  THIS->exec,nom,THIS->fi[file_pos].param);

	} else {
	  sprintf(exec_buffer,"%s %s",THIS->exec,nom);
	}
	
	if( ligneExecuter(exec_buffer,msg) != 0 ) {
	  fprintf(stderr,"%s\n",msg);
	  return -1;
	}
	
	if( remove(nom) != 0 ) {
	  fprintf(stderr,
		  "Warning: Cannot remove file %s\n",nom);
	}
      }
    }
  }

  return 0;
}

/*----------------------------------------------------------------------------*\
   Nom   : extraction
   But   : Extraire un ensemble de fichier d'un fichier cmcarc.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

static int extraction( cmcarc THIS)
{
	int i,j,sauter,fini,res;
	long long pos;
	pcmc fica;
	pcmc ficao;
	enum pcmc_err en;
	char *nom;
        char *key_filename;
	struct hashtable  *file_occurence;  /* a hashtable to note the occurence of files
					       in cmcarc archive.  Used for --sequence and 
					       --last options */
	hash_value* occured;

	res = 0;
	
	if( THIS->buffd != -1 ) {
		THIS->lbuff = THIS->buffd;
	} else {
		THIS->lbuff = THIS->xstdin ? CMCARC_STDBUFF : CMCARC_DSKBUFF;
	}

	if( pcmcAllouer(&fica,THIS->lbuff) != PCMCE_OK ) {
		fprintf(stderr,"Not enough memory.\n");
		return -1;
	}

	if( pcmcAllouer(&ficao,CMCARC_STDBUFF) != PCMCE_OK ) {
		fprintf(stderr,"Not enough memory.\n");
		fica->liberer(fica);
		return -1;
	}

	if( THIS->xstdin ) {
		fica->xstdin=1;
		if( fica->fileOuvrir(fica,stdin,0) != PCMCE_OK ) {
			fica->liberer(fica);
			ficao->liberer(ficao);
			return -1;
		}
	} else {
		if( fica->ouvrir(fica,THIS->fichier,"rb") != PCMCE_OK ) {
			fica->liberer(fica);
			ficao->liberer(ficao);
			return -1;
		}
	}

	if( THIS->xstdout ) {
		if( ficao->fileOuvrir(ficao,stdout,0) != PCMCE_OK ) {
			fica->fermer(fica);
			fica->liberer(fica);
			ficao->liberer(ficao);
			return -1;
		}

		if ( !THIS->xstdout_nohdr ) {
			if( ficao->signEcrire(ficao) != PCMCE_OK ) {
				fica->fermer(fica);
				ficao->fermer(ficao);
				fica->liberer(fica);
				ficao->liberer(ficao);
				return -1;
			}
		}
	}

	if( fica->signLire(fica,&cmcarchs_size) != PCMCE_OK ) {
		fica->fermer(fica);
		ficao->fermer(ficao);
		fica->liberer(fica);
		ficao->liberer(ficao);
		return -1;
	}

	if ( THIS->skip ) {
		if( fica->seek && (pos=ftell64(fica->fi)) == -1 ) {
			fprintf(stderr,
				"Warning: Cannot get current position in file %s, %s.\n",
				fica->nfi,strerror(errno));
		}
		fprintf(stderr, "Skipping first %lld bytes ...\n", THIS->skip);
		if( fseek64(fica->fi,THIS->skip,SEEK_SET) == -1 ) {
			return PCMCE_SEEK;
		}
	}

	if( fica->seek && (pos=ftell64(fica->fi)) == -1 ) {
		fprintf(stderr,
			"Warning: Cannot get current position in file %s, %s.\n",
			fica->nfi,strerror(errno));
	}

	nom = "nil";
	fini = 0;

	if(THIS->sequence || THIS->last) {
	  /* initialize the hashtable */
	  file_occurence = create_hashtable(16, hash_from_key_fn, keys_equal_fn);
	}

	do {  /* On revient ici apres un scan */

		while( (en=fica->enteteLire(fica)) == PCMCE_OK ) {

			sauter = 0;
			nom = fica->entete.param+1;

			if( ( strcmp(nom-1,PCMC_SIGN_IFI_CMCARC) == 0 ) ||
			    ( strcmp(nom-1,PCMC_SIGN_CMCARC) == 0 ) || 
			    ( strcmp(nom-1,PCMC_SIGN_CMCARC_OLD4) == 0 ) ) {
				sauter = 1;
			} else {
				i = THIS->ifi + 1;
				for( j=0; j<=THIS->ifi; j++ ) {

					if( expregComparer(THIS->fi[j].expn,nom) ) {
						THIS->fi[j].match = 1;
						i = j;
					}
				}

				if( THIS->ifi != -1 && i > THIS->ifi ) {
					sauter = 1;
				}
			}

			if( sauter ) {
				if( fica->donneesSauter(fica) != PCMCE_OK ) {
					fprintf(stderr,
     				"ERROR: error in %s\n",fica->entete.param+1);
					fica->fermer(fica);
					ficao->fermer(ficao);
					fica->liberer(fica);
					ficao->liberer(ficao);
					return PCMCE_ERR;
				}

				continue;
			} else if(THIS->sequence || THIS->last) {

				  if((occured=hashtable_search(file_occurence, nom))==NULL) {
			            /* first occurence of file */
				    occured=malloc(sizeof(hash_value));
				    occured->occurence=1;
				    if(!hashtable_insert(file_occurence, strdup(nom), occured)) {
				      fprintf(stderr, "Error occured while inserting into hashtable\n");
				      return -1;
				    }
				  } else {
			            /* subsequent occurence of file */
				    occured->occurence++;  /* no need to re-insert */
				  }
				  
				  if(cmcarc_version == VERSION4)
				    /* occured->pos = ftell64(fica->fi) - fica->nbr_header - 16; */
				    occured->pos = ftell64(fica->fi) - fica->nbr_header - 8;
				  else
				    /* occured->pos = ftell64(fica->fi) - fica->nbr_header - 32; */
				    occured->pos = ftell64(fica->fi) - fica->nbr_header - 16;

				  if(THIS->sequence != occured->occurence || THIS->last) {
				    /* this is not the occurence we want to extract */
				    sauter=1;
				  }
			}

			if(sauter) {
    			  if( fica->donneesSauter(fica) != PCMCE_OK ) {
      			    fprintf(stderr,
				"ERROR: error in %s\n",fica->entete.param+1);
      			    fica->fermer(fica);
      			    ficao->fermer(ficao);
      			    fica->liberer(fica);
      			    ficao->liberer(ficao);
      			    return PCMCE_ERR;
			  }
    			} else {
			  res = extract_this(THIS, fica, ficao, i);
			  if(res==2)
			    continue;     /* warning, extract next file */ 
			  else if(res<0)
			    return res;   /* error, abort */

			  /* res=0: ok, res=1: warning, go on */
			}
		}

		if( en == PCMCE_EOF ) {
			fini = 1;
		} else {

			if( fica->enteteScan(fica,pos) != PCMCE_OK ) {
				fica->fermer(fica);
				ficao->fermer(ficao);
				fica->liberer(fica);
				ficao->liberer(ficao);
				fprintf(stderr,"Scanning error after %s.\n",nom);
				return -1;
			}

			res = -2;
			fica->pos = PCMCP_ENTETE;
			fprintf(stderr, "Warning: skipping from byte offset %lld to %ld.\n",
				pos, ftell64(fica->fi));
			
			if( fica->seek && (pos=ftell(fica->fi)) == -1 ) {
				fprintf(stderr,
					"Warning: Cannot get current position in file %s, %s.\n",
					fica->nfi,strerror(errno));
				fica->fermer(fica);
				ficao->fermer(ficao);
				fica->liberer(fica);
				ficao->liberer(ficao);
				return -1;
			}
		}

	} while( !fini );

	if(THIS->last && hashtable_count(file_occurence) > 0) {
	  /* if the --last switch is used, 
	   * and there are actually files that need to be de-archived, 
	   * go through them in the hashtable and de-archive */
	  
	  struct hashtable_itr *itr;    /* iterate through hash_table */
	  hash_value* val_occurence;    /* value is occurence of file in the archive, 
					   important is the position */
	  
	  itr = hashtable_iterator(file_occurence);
	  do {
	    key_filename = hashtable_iterator_key(itr);
	    val_occurence = hashtable_iterator_value(itr);
	    
	    fica->pos=PCMCP_ENTETE;
	    if( fseek64(fica->fi,val_occurence->pos,SEEK_SET) == -1 ) {
	      fprintf(stderr, "Error: Can't seek in file, maybe due to --last switch?\n");
	      return -1;
	    }
	    
	    if((en=fica->enteteLire(fica)) != PCMCE_OK ) {
	      fprintf(stderr, "Error: Can't read header in file, maybe due to --last switch?\n");
	      return -1;
	    }

	    res = extract_this(THIS, fica, ficao, i);
	    if(res==2)
		continue;     /* warning, extract next file */ 
	    else if(res<0)
	   	return res;   /* error, abort */

	    /* res=0: ok, res=1: warning, go on */

	  } while (hashtable_iterator_advance(itr));
	  free(itr);
	}
	


	if( THIS->xstdout ) {
		ficao->fermer(ficao);
		ficao->liberer(ficao);
	}

	/* Verifie si certains fichiers n'ont pas ete trouves */
	if( THIS->inform || THIS->match_exit ) {
		for( i=0; i<=THIS->ifi; i++ ) {

			if( !THIS->fi[i].match ) {
				fprintf(stderr,
					"Warning: %s : file not found, or regular expression do not match any archived file.\n",
					THIS->fi[i].nom);
				if ( THIS->match_exit ) {
					res = 1;
				}
			}
		}
	}

	return res;
}


/*----------------------------------------------------------------------------*\
   Nom   : nomPreparer
   But   : Preparer le nom de fichier a ecrire dans un entete en fonction
           des options pref et path.
\*----------------------------------------------------------------------------*/

static void nomPreparer( char *nom, const char *n,
                         const char *pref, const char* post, const int path)
{
	char *q,*p;

	p = (char *)n;

	if( path ) {
		while( (q=strchr(p,'/')) != NULL ) { /* Garder seulement le basename */
			p = q+1;
		}
	}

/*	if( *pref != '\0' ) {
		sprintf(nom,"%s.%s",pref,p);
	} else {
		strcpy(nom,p);
	} */

        strcpy(nom,p);
        if( *pref != '\0' )
          sprintf(nom,"%s.%s",pref,strdup(nom));
        if( *post != '\0')
          sprintf(nom,"%s.%s",strdup(nom),post);
}
