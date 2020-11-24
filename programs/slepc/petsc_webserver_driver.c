#define  _POSIX_C_SOURCE 200809L
#include "petsc_webserver.h"
#include <stdio.h>
#include <petscviewersaws.h>
#include <unistd.h>
#include <signal.h>

static const char help[] = "PETSc webserver: \n";
/*
  "Usage:\n"
  "Options:\n"
  "-file [filename] : input file\n"
  "-type [TCPACCEPT,TCPCONNECT,TCPRETRANS,TCPLIFE,TCPCONNLAT] : what sort of input file\n"
  "--buffer_capcity [capacity] : (optional, default 10,000) size of the buffer (number of entries)\n"
  "--polling_interval [interval] : (optional, default 2.5) how many seconds to "
  "       wait before checking the file for more data after reaching the end?\n"
  "--url_filename [url_filename] : (optional) filename to write the URL of the "
  "       SAWs server to. Possibly useful if you want to have another program wait for "
  "       that entry to be written then email/otherwise send it to the user from a remote system.\n";
*/

entry_buffer   buf;
char           *line;
file_wrapper   input;
process_statistics pstats;

PetscErrorCode handle_line(char *line, PetscInt nentry, InputType input_type, PetscInt mypid,
			   tcpaccept_entry *accept_entry, tcpconnect_entry *connect_entry,
			   tcpconnlat_entry *connlat_entry, tcplife_entry *life_entry,
			   tcpretrans_entry *retrans_entry, PetscBool *ignore_entry,
			   process_statistics *pstats)
{
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  
  switch (input_type) {
    case TCPACCEPT:
      //ierr = create_tcpaccept_entry_bag(accept_entry,bagptr,nentry);CHKERRQ(ierr);
      ierr = tcpaccept_entry_parse_line(accept_entry,line); if (ierr) break;
      if ((accept_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      ierr = process_statistics_add_accept(pstats,accept_entry);CHKERRQ(ierr);
      break;
    case TCPCONNECT:
      ierr = tcpconnect_entry_parse_line(connect_entry,line);if (ierr) break;
      //PetscPrintf(PETSC_COMM_WORLD,"parsed bag %D\n",nentry);
      if ((connect_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      ierr = process_statistics_add_connect(pstats,connect_entry);CHKERRQ(ierr);
      break;
    case TCPCONNLAT:
      ierr = tcpconnlat_entry_parse_line(connlat_entry,line);if (ierr) break;
      if ((connlat_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      ierr = process_statistics_add_connlat(pstats,connlat_entry);CHKERRQ(ierr);
      break;
    case TCPLIFE:
      ierr = tcplife_entry_parse_line(life_entry,line);if (ierr) break;
      if ((life_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      ierr = process_statistics_add_life(pstats,life_entry);CHKERRQ(ierr);
      break;
    case TCPRETRANS:
      ierr = tcpretrans_entry_parse_line(retrans_entry,line);if (ierr) break;
      if ((retrans_entry)->pid == mypid) {
	/* if the traffic came from this program, don't upload it to SAWs */
	*ignore_entry = PETSC_TRUE;
      }
      ierr = process_statistics_add_retrans(pstats,retrans_entry);CHKERRQ(ierr);
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
  fclose(input.file);
  process_statistics_destroy(&pstats);
  PetscFinalize();
  exit(sig_num);
}
  

int main(int argc, char **argv)
{
  PetscErrorCode ierr;
  PetscBag       bag;
  size_t         buf_capacity,linesize,nread;
  PetscInt       N,ires,nentry,mypid,rank,size,num_pid,i,*pids;
  PetscReal      polling_interval;
  char           filename[PETSC_MAX_PATH_LEN],url_filename[PETSC_MAX_PATH_LEN],sawsurl[256];
  char           tempfile_path[] = "/tmp/petsc_webserver_XXXXXX",output_filename[PETSC_MAX_PATH_LEN];
  FILE           *output;
  PetscBool      has_filename,has_filename2,ignore_entry;
  InputType      input_type;
  tcpaccept_entry accept_entry;
  tcpconnect_entry connect_entry;
  tcpconnlat_entry connlat_entry;
  tcplife_entry life_entry;
  tcpretrans_entry retrans_entry;
  PetscViewer     viewer;
  process_data       *pdata;
  process_data_summary *psumm;
  ierr = PetscInitialize(&argc,&argv,NULL,help);if (ierr) return ierr;
  ierr = register_mpi_types();CHKERRQ(ierr);
  mypid = getpid();
  MPI_Comm_rank(PETSC_COMM_WORLD,&rank);
  MPI_Comm_size(PETSC_COMM_WORLD,&size);
  line = NULL;
  linesize = 0;
  SAWs_Initialize();
  SAWs_Set_Use_Logfile("SAWs.log");
  SAWs_Get_FullURL(sizeof(sawsurl),sawsurl);
  PetscPrintf(PETSC_COMM_WORLD,"SAWs is uploading to %s from process number %D\n",sawsurl,mypid);
  ierr = PetscOptionsGetString(NULL,NULL,"-file",filename,PETSC_MAX_PATH_LEN,&has_filename);CHKERRQ(ierr);
  if (!has_filename) {
    SETERRQ(PETSC_COMM_WORLD,1,"Must provide a filename (-file)");
  }
  ierr = PetscOptionsGetString(NULL,NULL,"-o",output_filename,PETSC_MAX_PATH_LEN,&has_filename);CHKERRQ(ierr);
  if (!has_filename) {
    ierr = PetscOptionsGetString(NULL,NULL,"--output",output_filename,PETSC_MAX_PATH_LEN,&has_filename2);CHKERRQ(ierr);
  }

  if (has_filename || has_filename2) {
    if (!rank) {
      output = fopen(output_filename,"w");
    }
  } else {
    output = stdout;
  }
  
  ignore_entry = PETSC_FALSE;
  N = 10000;
  ierr = PetscOptionsGetInt(NULL,NULL,"--buffer_capacity",&N,&has_filename);CHKERRQ(ierr);
  polling_interval = 2.5;
  ierr = PetscOptionsGetReal(NULL,NULL,"--polling_interval",&polling_interval,&has_filename);
  
  ierr = PetscOptionsGetString(NULL,NULL,"--url_filename",url_filename,PETSC_MAX_PATH_LEN,&has_filename);
  if (!rank) {
    if (has_filename) {
      FILE *url_file;
      url_file = fopen(url_filename,"w");
      fprintf(url_file,"%s\n",sawsurl);
      fclose(url_file);
    }
  }
  
  buf_capacity = (size_t)N;
  ierr = buffer_create(&buf,buf_capacity);CHKERRQ(ierr);
  signal(SIGINT,sigint_handler);
  ierr = PetscOptionsGetEnum(NULL,NULL,"-type",InputTypes,(PetscEnum*)&input_type,&has_filename);CHKERRQ(ierr);
  input.file = fopen(filename,"r");
  getline(&line,&linesize,input.file);
  getline(&line,&linesize,input.file);
  nentry = 0;
  ierr = process_statistics_init(&pstats);CHKERRQ(ierr);
  //PetscPrintf(PETSC_COMM_WORLD,"Creating SAWs viewer.\n");
  ierr = PetscViewerSAWsOpen(PETSC_COMM_WORLD,&viewer);CHKERRQ(ierr);
  while((nread = getline(&line,&linesize,input.file)) != -1) {
    
    ierr = handle_line(line,nentry,input_type,mypid,&accept_entry,
		       &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		       &ignore_entry,&pstats);CHKERRQ(ierr);
    if (ignore_entry) {
      PetscFPrintf(PETSC_COMM_WORLD,stderr,"Ignoring entry\n");
      continue;
    }
    

    ++nentry;
  }
  PetscFPrintf(PETSC_COMM_WORLD,stderr,"Handled %D entries.\n",nentry);

  ierr = process_statistics_num_entries(&pstats,&num_pid);CHKERRQ(ierr);
  ierr = PetscCalloc1(num_pid,&pids);CHKERRQ(ierr);
  ierr = PetscCalloc1(num_pid,&pdata);CHKERRQ(ierr);
  //ierr = PetscCalloc1(num_pid,&psumm);CHKERRQ(ierr);
  ierr = process_statistics_get_all(&pstats,pdata,pids);CHKERRQ(ierr);
  for (i=0; i<num_pid; ++i) {
    ierr = create_process_summary_bag(&psumm,&bag,rank,i);CHKERRQ(ierr);
    ierr = process_data_summarize(pids[i],&pdata[i],psumm);CHKERRQ(ierr);
    ires = buffer_try_insert(&buf,bag);
    if (ires == -1) {
      PetscFPrintf(PETSC_COMM_WORLD,stderr,"Error: buffer is full! Try increasing the capacity. Discarding this entry with pid %D and comm %s\n.",pids[i],psumm->comm);
    } 
  }

  /*if (!rank) {
    while (!buffer_empty(&buf)) {
      ierr = buffer_get_item(&buf,&bag);CHKERRQ(ierr);
      ierr = PetscBagGetData(bag,(void**)&psumm);CHKERRQ(ierr);
      ierr = summary_view(stdout,psumm);CHKERRQ(ierr);
      ierr = buffer_pop(&buf);CHKERRQ(ierr);
    }
  }*/
  
  ierr = buffer_gather_summaries(&buf);CHKERRQ(ierr);
  
  
  if (!rank) {
    while (!buffer_empty(&buf)) {
      ierr = buffer_get_item(&buf,&bag);CHKERRQ(ierr);
      ierr = PetscBagGetData(bag,(void**)&psumm);CHKERRQ(ierr);
      ierr = summary_view(output,psumm);CHKERRQ(ierr);
      ierr = buffer_pop(&buf);CHKERRQ(ierr);
    }
  }
    
  MPI_Barrier(PETSC_COMM_WORLD);

  size_t old_nentry = nentry;
  /* done with the file; now wait for more data; */
  while (PETSC_TRUE) {
    if (has_new_data(&input)) {
      while ((nread = getline(&line,&linesize,input.file)) != -1) {
	ierr = handle_line(line,nentry,input_type,mypid,&accept_entry,
		       &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
			   &ignore_entry,&pstats);CHKERRQ(ierr);
	if (ignore_entry) {
	  PetscPrintf(PETSC_COMM_WORLD,"Ignoring entry\n");
	  continue;
	}
	
	if (ierr) {
	  PetscPrintf(PETSC_COMM_WORLD,"Handled %D entries.\n",nentry);
	  return ierr;
	}
	/*
	ires = buffer_try_insert(&buf,bag);
	if (ires == -1) {
	  PetscPrintf(PETSC_COMM_WORLD,"Error: buffer is full! Try increasing the capacity. Discarding this entry.\n");
	} else {
	  ++nentry;
	  }*/
	++nentry;
      }
      PetscPrintf(PETSC_COMM_WORLD,"Handled %D entries.\n",nentry - old_nentry);
      ierr = process_statistics_num_entries(&pstats,&num_pid);CHKERRQ(ierr);
      ierr = PetscRealloc(num_pid * sizeof(PetscInt),&pids);CHKERRQ(ierr);
      ierr = PetscRealloc(num_pid * sizeof(process_data),&pdata);CHKERRQ(ierr);
      ierr = process_statistics_get_all(&pstats,pdata,pids);CHKERRQ(ierr);

      for (i=0; i<num_pid; ++i) {
	ierr = create_process_summary_bag(&psumm,&bag,rank,i);CHKERRQ(ierr);
	ierr = process_data_summarize(pids[i],&pdata[i],psumm);CHKERRQ(ierr);
	ires = buffer_try_insert(&buf,bag);
	if (ires == -1) {
	  PetscFPrintf(PETSC_COMM_WORLD,stderr,"Error: buffer is full! Try increasing the capacity. Discarding this entry with pid %D and comm %s\n.",pids[i],psumm->comm);
	}
      }

      ierr = buffer_gather_summaries(&buf);CHKERRQ(ierr);

      if (!rank) {
	while (!buffer_empty(&buf)) {
	  ierr = buffer_get_item(&buf,&bag);CHKERRQ(ierr);
	  ierr = PetscBagGetData(bag,(void**)&psumm);CHKERRQ(ierr);
	  ierr = summary_view(output,psumm);CHKERRQ(ierr);
	  ierr = buffer_pop(&buf);CHKERRQ(ierr);
	}
      }
      old_nentry = nentry;
      /* wait for new entries */
      ierr = PetscSleep(polling_interval);CHKERRQ(ierr);
    }
  }/* end main event loop */
  //SAWs_Finalize();
  ierr = PetscFree(pids);CHKERRQ(ierr);
  ierr = PetscFree(pdata);CHKERRQ(ierr);
  PetscFinalize();
  return 0;
}
