#include <petsc.h>
#include <petscsys.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <Python.h>
#include <execinfo.h>
#include <signal.h>

#define BACKTRACE_DEPTH 20

void handler(int sig) {
  void *bt[BACKTRACE_DEPTH];
  size_t size;

  size = backtrace(bt,BACKTRACE_DEPTH);

  fprintf(stderr, "Error on launcher: signal %d:\n", sig);
  backtrace_symbols_fd(bt,size,STDERR_FILENO);
  exit(1);
}

#define CHECK_TOKEN(str,tok,i) do {		\
  if (!tok) { \
    PetscFPrintf(PETSC_COMM_WORLD,stderr,"Child failed to find expected token number %D in string %s\n",i,str); \
  return i; \
  }\
  } while(0)


int main(int argc, char **argv)
{
  char path[PETSC_MAX_PATH_LEN],server_path[PETSC_MAX_PATH_LEN],
    server_input_file[PETSC_MAX_PATH_LEN],port[10];
  wchar_t wspath[PETSC_MAX_PATH_LEN],wsifile[PETSC_MAX_PATH_LEN],wf[3],wp[3],wport[10];
  char *tok;
  const char sep[2] = " ";
  MPI_Comm parent;
  PetscInt rank;
  MPI_Status stat;
  FILE     *server;
  MPI_Init(&argc,&argv);
  signal(SIGSEGV,handler);
  MPI_Comm_get_parent(&parent);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  if (parent != MPI_COMM_NULL) {
    MPI_Recv(path,PETSC_MAX_PATH_LEN,MPI_CHAR,0,0,parent,&stat);
    if (!rank) {
      tok = strtok(path,sep);
      CHECK_TOKEN(path,tok,0);
      if (strcmp(tok,"python3") != 0) {
        fprintf(stderr,"ERROR: First token of broadcast string must be python3, not %s\n",tok);
	exit(1);
      }
      tok = strtok(NULL,sep);
      CHECK_TOKEN(path,tok,1);
      PetscStrncpy(server_path,tok,PETSC_MAX_PATH_LEN);
      server = fopen(server_path,"r");
      swprintf(wspath,PETSC_MAX_PATH_LEN,L"%hs",server_path);
      tok = strtok(NULL,sep);
      CHECK_TOKEN(path,tok,2);
      if (strcmp(tok,"-f") != 0) {
	SETERRQ1(PETSC_COMM_WORLD,1,"Third token of broadcast string must be -f, not %s\n",tok);
      }
      tok = strtok(NULL,sep);
      CHECK_TOKEN(path,tok,3);
      PetscStrncpy(server_input_file,tok,PETSC_MAX_PATH_LEN);
      swprintf(wsifile,PETSC_MAX_PATH_LEN,L"%hs",server_input_file);
      tok = strtok(NULL,sep);
      CHECK_TOKEN(path,tok,4);
      
      if (strcmp(tok,"-p") != 0) {
	SETERRQ1(PETSC_COMM_WORLD,1,"Fifth token of broadcast string must be -p, not %s\n",tok);
      }
      tok = strtok(NULL,sep);
      CHECK_TOKEN(path,tok,5);
      PetscStrncpy(port,tok,10);
      swprintf(wport,10,L"%hs",port);
      swprintf(wp,3,L"%hs","-p");
      swprintf(wf,3,L"%hs","-f");
      wchar_t *wargv[] = {wspath,wf,wsifile,wp,wport};
      Py_SetProgramName(wspath);
      Py_Initialize();
      PySys_SetArgvEx(5,wargv,1);
      PyRun_SimpleFile(server,server_path);
      if (!Py_IsInitialized()){
	fprintf(stderr,"Error, Python not initialized\n");
      } else {
	fprintf(stderr,"The Python program is initialized to %s\n",server_path);
        
      }
      
    }
  }
      
  MPI_Finalize();
  return 0;
}
