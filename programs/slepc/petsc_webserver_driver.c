#define  _POSIX_C_SOURCE 200809L
#include "petsc_webserver.h"
#include <stdio.h>
#include <petscviewersaws.h>
#include <unistd.h>
static const char help[] = "PETSc webserver: \n"
  "Usage:\n"
  "Options:\n"
  "-file [filename] : input file\n"
  "-type [TCPACCEPT,TCPCONNECT,TCPRETRANS,TCPLIFE,TCPCONNLAT] : what sort of input file\n"
  "--buffer_capcity [capacity] : (optional, default 10,000) size of the buffer (number of entries)\n";

int main(int argc, char **argv)
{
  PetscErrorCode ierr;
  entry_buffer   buf;
  //PetscBag       bag;
  size_t         buf_capacity,linesize,nread;
  PetscInt       N,ires,nentry,mypid,i;
  file_wrapper   input;
  char           filename[PETSC_MAX_PATH_LEN],*line,sawsurl[256];
  PetscBool      has_filename,ignore_entry;
  InputType      input_type;
  tcpaccept_entry *accept_entry;
  tcpconnect_entry *connect_entry;
  tcpconnlat_entry *connlat_entry;
  tcplife_entry *life_entry;
  tcpretrans_entry *retrans_entry;
  PetscViewer     viewer;
  ierr = PetscInitialize(&argc,&argv,NULL,help);if (ierr) return ierr;
  mypid = getpid();
  line = NULL;
  linesize = 0;
  SAWs_Initialize();
  SAWs_Get_FullURL(sizeof(sawsurl),sawsurl);
  PetscPrintf(PETSC_COMM_WORLD,"SAWs is uploading to %s from process number %D\n",sawsurl,mypid);
  ierr = PetscOptionsGetString(NULL,NULL,"-file",filename,PETSC_MAX_PATH_LEN,&has_filename);CHKERRQ(ierr);
  if (!has_filename) {
    SETERRQ(PETSC_COMM_WORLD,1,"Must provide a filename (-file)");
  }
  ignore_entry = PETSC_FALSE;
  N = 10000;
  ierr = PetscOptionsGetInt(NULL,NULL,"--buffer_capacity",&N,&has_filename);CHKERRQ(ierr);
  PetscBag bag[N];
  //buf_capacity = (size_t)N;
  //ierr = buffer_create(&buf,buf_capacity);CHKERRQ(ierr);
  ierr = PetscOptionsGetEnum(NULL,NULL,"-type",InputTypes,(PetscEnum*)&input_type,&has_filename);CHKERRQ(ierr);
  PetscPrintf(PETSC_COMM_WORLD,"Opening file\n");
  input.file = fopen(filename,"r");
  getline(&line,&linesize,input.file);
  getline(&line,&linesize,input.file);
  nentry = 0;
  //PetscPrintf(PETSC_COMM_WORLD,"Creating SAWs viewer.\n");
  ierr = PetscViewerSAWsOpen(PETSC_COMM_WORLD,&viewer);CHKERRQ(ierr);
  char obj_name[100];
  while((nread = getline(&line,&linesize,input.file)) != -1) {
    //PetscPrintf(PETSC_COMM_WORLD,"Parsing line number %D\n",nentry);
    /* TODO: preallocate the PetscBag instances in the buffer constructor */
    switch (input_type) {
    case TCPACCEPT:
      ierr = create_tcpaccept_entry_bag(&accept_entry,&bag[nentry],nentry);CHKERRQ(ierr);
      ierr = tcpaccept_entry_parse_line(accept_entry,line); if (ierr) break;
      if (accept_entry->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	ignore_entry = PETSC_TRUE;
      }
      break;
    case TCPCONNECT:
      //PetscPrintf(PETSC_COMM_WORLD,"creating bag %D\n",nentry);
      ierr = create_tcpconnect_entry_bag(&connect_entry,&bag[nentry],nentry);CHKERRQ(ierr);
      //PetscPrintf(PETSC_COMM_WORLD,"created bag %D\n",nentry);
      ierr = tcpconnect_entry_parse_line(connect_entry,line);if (ierr) break;
      //PetscPrintf(PETSC_COMM_WORLD,"parsed bag %D\n",nentry);
      if (connect_entry->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	ignore_entry = PETSC_TRUE;
      }
      break;
    case TCPCONNLAT:
      ierr = create_tcpconnlat_entry_bag(&connlat_entry,&bag[nentry],nentry);CHKERRQ(ierr);
      ierr = tcpconnlat_entry_parse_line(connlat_entry,line);if (ierr) break;
      if (connlat_entry->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	ignore_entry = PETSC_TRUE;
      }
      break;
    case TCPLIFE:
      ierr = create_tcplife_entry_bag(&life_entry,&bag[nentry],nentry);CHKERRQ(ierr);
      ierr = tcplife_entry_parse_line(life_entry,line);if (ierr) break;
      if (life_entry->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	ignore_entry = PETSC_TRUE;
      }
      break;
    case TCPRETRANS:
      ierr = create_tcpretrans_entry_bag(&retrans_entry,&bag[nentry]);CHKERRQ(ierr);
      ierr = tcpretrans_entry_parse_line(retrans_entry,line);if (ierr) break;
      if (retrans_entry->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	ignore_entry = PETSC_TRUE;
      }
      break;
    }
    if (ignore_entry) {
      PetscPrintf(PETSC_COMM_WORLD,"Ignoring entry\n");
      //PetscBagDestroy(&bag);
      continue;
    }
    
    if (ierr) {
      //PetscBagDestroy(&bag[n);
      PetscPrintf(PETSC_COMM_WORLD,"Handled %D entries.\n",nentry);
      return ierr;
    }
    //ires = buffer_try_insert(&buf,bag);
    // if (ires == -1) {
    //   PetscPrintf(PETSC_COMM_WORLD,"Error: buffer is full! Try increasing the capacity. Discarding this entry.");
    //}
    //sprintf(obj_name,"entry_%d",nentry);
    //ierr = PetscObjectSetName((PetscObject)bag,obj_name);CHKERRQ(ierr);
    //PetscPrintf(PETSC_COMM_WORLD,"viewing bag %D\n",nentry);
    ierr = PetscBagView(bag[nentry],viewer);CHKERRQ(ierr);
    ierr = PetscBagView(bag[nentry],PETSC_VIEWER_STDOUT_WORLD);
    ++nentry;
    //PetscSAWsBlock();
    //ierr = PetscBagDestroy(&bag);CHKERRQ(ierr);
   
  }
  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);
  PetscPrintf(PETSC_COMM_WORLD,"Handled %D entries.\n",nentry);
  //ierr = buffer_destroy(&buf);CHKERRQ(ierr);
  for (i=0; i<nentry; ++i) {
    if (bag[i]) {
      PetscBagDestroy(&bag[i]);
    }
  }
  //SAWs_Finalize();
  PetscFinalize();
  return 0;
}
