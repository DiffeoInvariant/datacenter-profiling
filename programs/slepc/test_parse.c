#define  _POSIX_C_SOURCE 200809L
#include "petsc_webserver.h"
#include <stdio.h>

int main(int argc, char **argv)
{
  PetscBag        bag;
  tcpconnect_entry *entry;
  PetscErrorCode ierr;
  char           filename[PETSC_MAX_PATH_LEN], *line;
  PetscBool      has_filename;
  FILE           *input;
  size_t         linesize;
  ierr = PetscInitialize(&argc,&argv,NULL,NULL);  if (ierr) return ierr;
  ierr = PetscOptionsGetString(NULL,NULL,"-file",filename,PETSC_MAX_PATH_LEN,&has_filename);CHKERRQ(ierr);
  if (!has_filename) {
    SETERRQ(PETSC_COMM_WORLD,1,"Must provide a filename (-file)");
  }

  ierr = create_tcpconnect_entry_bag(&entry,&bag);CHKERRQ(ierr);
  input = fopen(filename,"r");
  getline(&line,&linesize,input);/* first line */
  getline(&line,&linesize,input);
  PetscPrintf(PETSC_COMM_WORLD,"%s",line);
  while(getline(&line,&linesize,input)) {
    PetscPrintf(PETSC_COMM_WORLD,"Parsing line %s",line);
    ierr = tcpconnect_entry_parse_line(entry,line);CHKERRQ(ierr);
    ierr = PetscBagView(bag,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  }
  PetscBagDestroy(&bag);
  PetscFinalize();
  return 0;
}

  
