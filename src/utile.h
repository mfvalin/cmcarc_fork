/*-------------------------------------------------------------------------*/
/* utile.h -- interface de tout ce qui ne fait pas partie du standard ANSI */
/*-------------------------------------------------------------------------*/

#ifndef UTILE_H
#define UTILE_H

int fichierExiste( const char *fichier, char *msg);
int fichierInfo( const char *fichier, long long *lng, int *mode, long *uid, long *gid, long *mtime, char *msg, int dereference);
int fichierModeSet( const char *fichier, const int mode, char *msg);
int fichierTimeSet( const char *fichier, const long mtime, char *msg);

int modeRepertoire( const int mode);
int modeRegulier( const int mode);

int repertoireCreer( const char *nom, const int perm, char *msg);
/* int repertoireLister( const char *rep, char **ptr, char *noms,
                      const int imptr, const int imnoms, char *msg); */
int repertoireLister( const char *rep, char ***ptr, char *msg);

int cheminCreer( const char *nom, char *msg);

int expregCompiler( const char *exp);
int expregComparer( const int expn, const char *exp);
int expregLiberer( const int expn);

int stdinCopier( const char *fi);

int ligneExecuter( const char *li, char *msg);

#endif
