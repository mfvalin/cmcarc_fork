/*--------------------------------------------------------------------------*/
/* utile.c -- implantation de tout ce qui ne fait pas partie du std. ANSI */
/*--------------------------------------------------------------------------*/

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <dirent.h>

#include "utile.h"

#ifndef _SX
#ifdef __cplusplus
extern "C" {
#endif
#include <regex.h>
FILE *popen(const char *, const char *);
#ifdef __cplusplus
}
#endif
#else
#include <libgen.h>
#endif


/*----------------------------------------------------------------------------*\
   Nom   : fichierExiste
   But   : Verifier si un fichier existe sur disque.
   Retour: 0=non, 1=oui
\*----------------------------------------------------------------------------*/

int fichierExiste( const char *fichier, char *msg)
{
	struct stat64 st;
	int res;

	if( msg ) {       /* msg optionnel, passer NULL */
		msg[0]='\0';
	}

	res  = stat64(fichier,&st);

	if( res < 0 ) {
		if( msg ) {
			(void)strcpy(msg,strerror(errno));
		}

		return 0;
	}

	return 1;
}



/*----------------------------------------------------------------------------*\
   Nom   : fichierInfo
   But   : Obtenir la longeur et le mode d'un fichier sur disque.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

int fichierInfo( const char *fichier, long long *lng, int *mode, long *uid, long *gid, long *mtime, char *msg, int dereference)
{
	struct stat64 st;
	int res;

	if( msg ) {       /* msg optionnel, passer NULL */
		*msg = '\0';
	}

	if(dereference)
	  res = stat64(fichier,&st);
	else
	  res = lstat64(fichier,&st);   /* AF */

	if( res < 0 ) {
		if( msg ) {
			(void)strcpy(msg,strerror(errno));
		}

		return -1;
	}

	*lng = (long long)st.st_size;    /* error, should be long long, but it doesn't hurt AF */
	*mode = st.st_mode;
	*uid = (long)st.st_uid;
	*gid = (long)st.st_gid;
#ifdef __sgi
	*mtime = (long)st.st_mtim.tv_sec;
#else
	*mtime = (long)st.st_mtime;
#endif

	return 0;
}



/*----------------------------------------------------------------------------*\
   Nom   : fichierTimeSet
   But   : Changer le temps (date de modification) d'un fichier sur disque.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

int fichierTimeSet( const char *fichier, const long mtime, char *msg)
{
	struct utimbuf newtime;

	if( msg ) {       /* msg optionnel, passer NULL */
		*msg = '\0';
	}

	newtime.actime = mtime;
	newtime.modtime = mtime;

	if( utime(fichier,&newtime) == -1 ) {
		if( msg ) {
			(void)sprintf(msg,
				"Cannot change time of file %s, %s",
				fichier,strerror(errno));
		}
	
		return -1;
	}

	return 0;
}



/*----------------------------------------------------------------------------*\
   Nom   : fichierModeSet
   But   : Changer le mode (permissions) d'un fichier sur disque.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

int fichierModeSet( const char *fichier, const int mode, char *msg)
{
	if( msg ) {       /* msg optionnel, passer NULL */
		*msg = '\0';
	}

	if( chmod(fichier,mode) == -1 ) {
		if( msg ) {
			(void)sprintf(msg,
				"Cannot change mode of file %s, %s",
				fichier,strerror(errno));
		}
	
		return -1;
	}

	return 0;
}


/*----------------------------------------------------------------------------*\
   Nom   : modeRepertoire
   But   : Verifier si le mode d'un fichier indique qu'il s'agit d'un rep.
   Retour: 0=non, 1=oui.
\*----------------------------------------------------------------------------*/

int modeRepertoire(const int mode)
{
	return mode & S_IFDIR;
}


/*----------------------------------------------------------------------------*\
   Nom   : modeRegulier
   But   : Verifier si le mode d'un fichier indique qu'il est regulier.
   Retour: 0=non, 1=oui.
\*----------------------------------------------------------------------------*/

int modeRegulier( const int mode)
{
	return mode & S_IFREG;
}


/*----------------------------------------------------------------------------*\
   Nom   : repertoireCreer
   But   : Creer un repertoire.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

int repertoireCreer( const char *nom, const int perm, char *msg)
{
	int p;

	if( perm == -1 ) {
		p = 0755;
	} else {
		p = perm;
	}

	if( msg ) {       /* msg optionnel, passer NULL */
		*msg = '\0';
	}

	if( mkdir(nom,p) == -1 && errno != EEXIST ) {
		if( msg ) {
			(void)sprintf(msg,
				"Cannot create directory %s, %s",
				nom,strerror(errno));
		}

		return -1;
	}
	chmod(nom,perm);

	return 0;
}


#define UTILE_MAX_REPL 300

/*----------------------------------------------------------------------------*\
   Nom   : repertoireLister
   But   : Lister tous les fichiers d'un repertoire et de ses sous-repertoires.
   Retour: -1=Erreur, >=0 nombre d'entrees dans ptr.
\*----------------------------------------------------------------------------*/

#if 0
int repertoireLister( const char *rep, char *ptr[], char *noms,
                      const int imptr, const int imnoms, char *msg)
{
	FILE *p;
	char *pc, *max, *eol;
	char buff[UTILE_MAX_REPL];
	char cm[2000];
	int i;

	if( !msg ) {
		(void)fprintf(stderr,
			"Internal error: msg==NULL in repertoireLister.\n");
		return -1;
	}

	i = -1;
	*msg = '\0';
	pc = noms;
	max = noms + imnoms;
	(void)sprintf(cm,"find %s -type f -print",rep);

	if( (p = popen(cm,"r")) == NULL ) {
		(void)sprintf(msg,"Cannot open command \"%s\", %s\n",
			cm,strerror(errno));
		return -1;
	}

	while( fgets(buff,UTILE_MAX_REPL,p) ) {

		if( i == imptr ) {
			(void)sprintf(msg,
				"Internal error, pointer overflow in repertoireLister.\n");
			return -1;
		}

		eol = strchr(buff,'\n');

		if( eol == NULL ) {
			(void)sprintf(msg,
				"Internal error, no end of line in repertoireLister.\n");
			return -1;
		}

		*eol = '\0';

		if( pc + strlen(buff) + 1 > max ) {
			(void)sprintf(msg,
				"Internal error, buffer overflow in repertoireLister.\n");
			return -1;
		}

		ptr[++i] = pc;
		(void)strcpy(pc,buff);
		pc += strlen(buff)+1;
	}

	if( pclose(p) == -1 ) {
		(void)sprintf(msg,
			"Cannot close normally pipe from command \"%s\", %s.\n",
			cm,strerror(errno));
		return -1;
	}

	return i+1;
}
#endif

/*----------------------------------------------------------------------------*\
   Nom   : repertoireLister
   But   : Lister tous les fichiers d'un repertoire et de ses sous-repertoires.
   Retour: -1=Erreur, >=0 nombre d'entrees dans ptr.
\*----------------------------------------------------------------------------*/


/* reads directory rep, and lists its files into ptr.
 * Does dynamic mem allocation into rep.  Note: for 
 * version 4, it does not archive directories, so empty
 * directories are lost.
 *
 * returns the number of files in the dir (recursively)
 */

int repertoireListerHelper(const char *rep, char ***ptr, const int nbr, int* alloc) {
   int i=nbr;
   struct dirent* entry;  
   char** dptr=*ptr;

   DIR* dir=opendir(rep);
   if(!dir) {
     fprintf(stderr, "Can't open directory %s\n", rep);
     return 0;  /* no files added */
   }

   while((entry=readdir(dir))) {
     if(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0) 
       continue;
     else {
       struct stat myStat;
       char* pathName=(char*)malloc(strlen(rep) + entry->d_reclen + 2);
       sprintf(pathName, "%s/%s", rep, entry->d_name);

       if(lstat(pathName, &myStat)!=0) {
         fprintf(stderr, "Can't stat file/directory %s\n", pathName);
#if 0
       } else if(S_ISDIR(myStat.st_mode)) {
         /* if directory: DO NOT add directory, but list it recursively */
         int nbrSubDir=repertoireListerHelper(pathName, &dptr, i, alloc);
         /* if(nbrSubDir==-1) 
         return -1; */
         i+=nbrSubDir;
       } else if(S_ISREG(myStat.st_mode) || S_ISLNK(myStat.st_mode)) {
         if(i==*alloc) {
           /* grow */
           char** temp=(char**)malloc((*alloc) * sizeof(char*));
           memcpy(temp, dptr, (*alloc) * sizeof(char*));
           free(dptr);
           dptr=(char**)malloc(2 * (*alloc) * sizeof(char*));
           memcpy(dptr, temp, (*alloc) * sizeof(char*));
           free(temp);
           (*alloc)*=2;
         }
         dptr[i]=(char*)malloc(strlen(pathName)+1);
	 strcpy(dptr[i], pathName);
	 free(pathName);

         i++;
       }
#endif     
       } else if(S_ISREG(myStat.st_mode) || S_ISLNK(myStat.st_mode) || S_ISDIR(myStat.st_mode)) {
         if(i==*alloc) {
           /* grow */
           char** temp=(char**)malloc((*alloc) * sizeof(char*));
           memcpy(temp, dptr, (*alloc) * sizeof(char*));
           free(dptr);
           dptr=(char**)malloc(2 * (*alloc) * sizeof(char*));
           memcpy(dptr, temp, (*alloc) * sizeof(char*));
           free(temp);
           (*alloc)*=2;
         }
         dptr[i]=(char*)malloc(strlen(pathName)+1);
	 strcpy(dptr[i], pathName);

         i++;

         if(S_ISDIR(myStat.st_mode)) {
           int nbrSubDir=repertoireListerHelper(pathName, &dptr, i, alloc);
           i+=nbrSubDir;
         }

	 free(pathName);
       }
     }

   }
   closedir(dir);
   *ptr=dptr;
   return (i-nbr);
}
       

/*
 * Reads directory rep, and recursively lists its regular
 * file content into ptr.  Dynamic mem allocation.  
 * 
 * Return the number of files (>=0)   If rep cannot be
 * opened, -1 is returned and the error message is set to msg.
 */

int repertoireLister( const char *rep, char ***ptr, char *msg) {
   int alloc=1;
   char** dptr;
   int nbr;

   DIR* dir=opendir(rep);
   if(!dir) {
     sprintf(msg, "Can't open directory %s\n", rep);
     return -1;
   }
   closedir(dir);

   dptr=(char**)malloc(alloc*sizeof(char*));
   
   nbr=repertoireListerHelper(rep, &dptr, 0, &alloc);
   *ptr=dptr;
   return nbr;
}



/*----------------------------------------------------------------------------*\
   Nom   : cheminCreer
   But   : Verifier un chemin, creer les sous-repertoires au besoin.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

int cheminCreer( const char *nom, char *msg)
{
	struct stat s;
	char repc[300];
	char nm[300];
	char *p, *n;
	int res;

	if( !msg ) {
		(void)fprintf(stderr,
			"Internal error: msg==NULL in cheminCreer.\n");
		return -1;
	}

	(void)getcwd(repc,300);
	(void)strcpy(nm,nom);
	p = nm;
	
	if( p[0] == '/' ) {

		if( chdir("/") == -1 ) {
			(void)sprintf(msg,
				"Cannot chdir to root (/), %s",
				strerror(errno));
		}

		n = p+1;
	} else {
		n = p;
	}

	p = strchr(n,'/');
	res = 0;

	while( p != NULL ) {

		p[0] = '\0';
		
		if( stat(n,&s) == -1 ) {

			if( errno == ENOENT ) {
				if( mkdir(n,0755) == -1 ) {
					(void)sprintf(msg,
						"Cannot create directory %s in %s, %s",
						n,nom,strerror(errno));
					res = -1;
					break;
				}
			} else {
				(void)sprintf(msg,
					"Error while examining %s, %s",
					nom,strerror(errno));
				res = -1;
				break;
			}
		}

		if( chdir(n) == -1 ) {
			(void)sprintf(msg,
				"Cannot go to directory %s from %s, %s.",
				n,nom,strerror(errno));
			res = -1;
			break;
		}

		p[0] = '/';
		n = p+1;
		p = strchr(n,'/');
	}

	if( chdir(repc) == -1 ) {
		(void)sprintf(msg,
			"Cannot go back to directory %s, %s",
			repc,strerror(errno));
	}

	return res;
}

#define MAX_XREG 16000

static struct expreg_s {
#ifdef _SX
	char ch[200];
#else
	regex_t re;
#endif
	int free;
} Xreg[MAX_XREG];

static int Ixreg = -1;


/*----------------------------------------------------------------------------*\
   Nom   : expregCompiler
   But   : Allouer et compiler une expression reguliere.
   Retour: -1=Erreur, >=0 le numero de l'expression allouee.
\*----------------------------------------------------------------------------*/

int expregCompiler( const char *exp)
{
	char modif[1000];
#ifdef _SX
        char *regcmp_str;
#endif
	if( Ixreg >= MAX_XREG ) {
		return -1;
	}

#ifdef _SX
	(void)sprintf(modif,"^%s$",exp); 
	regcmp_str=regcmp(modif, (char *)0);
	(void)strcpy(Xreg[Ixreg+1].ch,regcmp_str);
	free(regcmp_str);
#else
	(void)sprintf(modif,"^%s$",exp);

	if( regcomp(&Xreg[Ixreg+1].re,modif,REG_EXTENDED) != 0 ) {
		return -1;
	}
#endif

	Xreg[Ixreg+1].free = 0;

	Ixreg++;
	return Ixreg;
}


/*----------------------------------------------------------------------------*\
   Nom   : expregComparer
   But   : Comparer une chaine a une expression reguliere.
   Retour: -1=Erreur, 0=match pas, 1=match.
\*----------------------------------------------------------------------------*/

int expregComparer( const int expn, const char *exp)
{
	if( expn > Ixreg ) {
		return -1;
	}

	if( Xreg[expn].free ) {
		return -1;
	}

#ifdef _SX
	if( regex(Xreg[expn].ch,exp) != NULL ) {
		return 1;
	} else {
		return 0;
	}
#else
	if( regexec(&Xreg[expn].re,exp,0,NULL,0) == 0 ) {
		return 1;
	} else {
		return 0;
	}
#endif
}


/*----------------------------------------------------------------------------*\
   Nom   : expregLiberer
   But   : Liberer une expression reguliere.
   Retour: 0=Ok
\*----------------------------------------------------------------------------*/

int expregLiberer( const int expn)
{
	Xreg[expn].free = 1;
	return 0;
}


/*----------------------------------------------------------------------------*\
   Nom   : stdinCopier
   But   : Copier stdin vers un fichier.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

int stdinCopier( const char *fi)
{
	FILE *pfich;
	char ligne[1000];

	if( (pfich=fopen64(fi,"w")) == NULL ) {
		(void)fprintf(stderr,
			"Cannot open file %s for writing, %s\n",
			fi,strerror(errno));
		return -1;
	}

	while( fgets(ligne,sizeof(ligne)-1,stdin) ) {

		if( fprintf(pfich,"%s\n",ligne) < 0 ) {

			(void)fprintf(stderr,
				"Error detected while writing to file %s, %s.\n",
				fi,strerror(errno));

			(void)fclose(pfich);
			return -1;
		}
	}

	if( fclose(pfich) != 0 ) {
		(void)fprintf(stderr,
			"Warning: Cannot close file %s, %s.\n",
			fi,strerror(errno));
	}

	return 0;
}


/*----------------------------------------------------------------------------*\
   Nom   : ligneExecuter
   But   : Passer au shell une ligne de commande a executer.
   Retour: 0=Ok, -1=Erreur.
\*----------------------------------------------------------------------------*/

int ligneExecuter( const char *li, char *msg)
{
	int r;

	if( msg ) {       /* msg optionnel, passer NULL */
		*msg = '\0';
	}

	if( (r=system(li)) != 0 ) {
		if( msg ) {
			(void)sprintf(msg,
				"Execution error in \"%s\", exit code %d-%d, %s",
				li,r>>8,r&255,strerror(errno));
		}
		
		return -1;
	}

	return 0;
}
