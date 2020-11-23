#define  _POSIX_C_SOURCE 200809L
#include "petsc_webserver.h"
#include <stdio.h>
#include <petscviewersaws.h>
#include <unistd.h>
#include <signal.h>

static const char help[] = "PETSc webserver: \n"
  "Usage:\n"
  "Options:\n"
  "-file [filename] : input file\n"
  "-type [TCPACCEPT,TCPCONNECT,TCPRETRANS,TCPLIFE,TCPCONNLAT] : what sort of input file\n"
  "--buffer_capcity [capacity] : (optional, default 10,000) size of the buffer (number of entries)\n"
  "--polling_interval [interval] : (optional, default 2.5) how many seconds to wait before checking the file for more data after reaching the end?\n";

entry_buffer   buf;
char           *line;


PetscErrorCode handle_line(char *line, PetscBag *bagptr, PetscInt nentry, InputType input_type, PetscInt mypid,
			   tcpaccept_entry **accept_entry, tcpconnect_entry **connect_entry,
			   tcpconnlat_entry **connlat_entry, tcplife_entry **life_entry,
			   tcpretrans_entry **retrans_entry, PetscBool *ignore_entry)
{
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  
  switch (input_type) {
    case TCPACCEPT:
      ierr = create_tcpaccept_entry_bag(accept_entry,bagptr,nentry);CHKERRQ(ierr);
      ierr = tcpaccept_entry_parse_line(*accept_entry,line); if (ierr) break;
      if ((*accept_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      break;
    case TCPCONNECT:
      //PetscPrintf(PETSC_COMM_WORLD,"creating bag %D\n",nentry);
      ierr = create_tcpconnect_entry_bag(connect_entry,bagptr,nentry);CHKERRQ(ierr);
      //PetscPrintf(PETSC_COMM_WORLD,"created bag %D\n",nentry);
      ierr = tcpconnect_entry_parse_line(*connect_entry,line);if (ierr) break;
      //PetscPrintf(PETSC_COMM_WORLD,"parsed bag %D\n",nentry);
      if ((*connect_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      break;
    case TCPCONNLAT:
      ierr = create_tcpconnlat_entry_bag(connlat_entry,bagptr,nentry);CHKERRQ(ierr);
      ierr = tcpconnlat_entry_parse_line(*connlat_entry,line);if (ierr) break;
      if ((*connlat_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      break;
    case TCPLIFE:
      ierr = create_tcplife_entry_bag(life_entry,bagptr,nentry);CHKERRQ(ierr);
      ierr = tcplife_entry_parse_line(*life_entry,line);if (ierr) break;
      if ((*life_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      break;
    case TCPRETRANS:
      ierr = create_tcpretrans_entry_bag(retrans_entry,bagptr);CHKERRQ(ierr);
      ierr = tcpretrans_entry_parse_line(*retrans_entry,line);if (ierr) break;
      if ((*retrans_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      break;
    }

  PetscFunctionReturn(0);
}

void sigint_handler(int sig_num)
{
  buffer_destroy(&buf);
  if (line) {
    free(line);
  }
  PetscFinalize();
  exit(sig_num);
}
  

int main(int argc, char **argv)
{
  PetscErrorCode ierr;
  PetscBag       bag;
  size_t         buf_capacity,linesize,nread;
  PetscInt       N,ires,nentry,mypid;
  PetscReal      polling_interval;
  file_wrapper   input;
  char           filename[PETSC_MAX_PATH_LEN],sawsurl[256];
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
  polling_interval = 2.5;
  ierr = PetscOptionsGetReal(NULL,NULL,"--polling_interval",&polling_interval,&has_filename);
  
  buf_capacity = (size_t)N;
  ierr = buffer_create(&buf,buf_capacity);CHKERRQ(ierr);
  signal(SIGINT,sigint_handler);
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
    
    /* TODO: preallocate the PetscBag instances in the buffer constructor */
    
    ierr = handle_line(line,&bag,nentry,input_type,mypid,&accept_entry,
		       &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		       &ignore_entry);CHKERRQ(ierr);
    if (ignore_entry) {
      PetscPrintf(PETSC_COMM_WORLD,"Ignoring entry\n");
      continue;
    }
    
    if (ierr) {
      PetscPrintf(PETSC_COMM_WORLD,"Handled %D entries.\n",nentry);
      return ierr;
    }
    ires = buffer_try_insert(&buf,bag);
     if (ires == -1) {
       PetscPrintf(PETSC_COMM_WORLD,"Error: buffer is full! Try increasing the capacity. Discarding this entry.");
     } else {
       ++nentry;
     }
  }
  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);
  PetscPrintf(PETSC_COMM_WORLD,"Handled %D entries.\n",nentry);
  while (!buffer_empty(&buf)) {
    ierr = buffer_get_item(&buf,&bag);CHKERRQ(ierr);
    ierr = PetscBagView(bag,viewer);CHKERRQ(ierr);
    ierr = PetscBagView(bag,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    ierr = buffer_pop(&buf);CHKERRQ(ierr);
  }
  size_t old_nentry = nentry;
  /* done with the file; now wait for more data; */
  while (PETSC_TRUE) {
    if (has_new_data(&input)) {
      while ((nread = getline(&line,&linesize,input.file)) != -1) {
	ierr = handle_line(line,&bag,nentry,input_type,mypid,&accept_entry,
		       &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		       &ignore_entry);CHKERRQ(ierr);
	if (ignore_entry) {
	  PetscPrintf(PETSC_COMM_WORLD,"Ignoring entry\n");
	  continue;
	}
	
	if (ierr) {
	  PetscPrintf(PETSC_COMM_WORLD,"Handled %D entries.\n",nentry);
	  return ierr;
	}
	ires = buffer_try_insert(&buf,bag);
	if (ires == -1) {
	  PetscPrintf(PETSC_COMM_WORLD,"Error: buffer is full! Try increasing the capacity. Discarding this entry.");
	} else {
	  ++nentry;
	}
      }
      PetscPrintf(PETSC_COMM_WORLD,"Handled %D entries.\n",nentry - old_nentry);
      while (!buffer_empty(&buf)) {
	ierr = buffer_get_item(&buf,&bag);CHKERRQ(ierr);
	ierr = PetscBagView(bag,viewer);CHKERRQ(ierr);
	ierr = PetscBagView(bag,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
	ierr = buffer_pop(&buf);CHKERRQ(ierr);
      }
      old_nentry = nentry;
      /* wait for new entries */
      ierr = PetscSleep(polling_interval);CHKERRQ(ierr);
    }
  }/* end main event loop */
  //SAWs_Finalize();
  PetscFinalize();
  return 0;
}
