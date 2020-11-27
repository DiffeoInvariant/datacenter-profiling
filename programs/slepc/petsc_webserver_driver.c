#define  _POSIX_C_SOURCE 200809L
#include "petsc_webserver.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <execinfo.h>

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
file_wrapper   input, accept_input, connect_input, connlat_input, life_input, retrans_input;
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
      if ((accept_entry)->pid == mypid || ierr) {
	/* if the traffic came from this program, don't upload it to the server */
	*ignore_entry = PETSC_TRUE;
	//break;
      }
      ierr = process_statistics_add_accept(pstats,accept_entry);CHKERRQ(ierr);
      break;
    case TCPCONNECT:
      ierr = tcpconnect_entry_parse_line(connect_entry,line);if (ierr) break;
      //PetscPrintf(PETSC_COMM_WORLD,"parsed bag %D\n",nentry);
      if ((connect_entry)->pid == mypid || ierr) {
	/* if the traffic came from this program, don't upload it to the server */
	*ignore_entry = PETSC_TRUE;
	break;
      }
      ierr = process_statistics_add_connect(pstats,connect_entry);CHKERRQ(ierr);
      break;
    case TCPCONNLAT:
      ierr = tcpconnlat_entry_parse_line(connlat_entry,line);if (ierr) break;
      if ((connlat_entry)->pid == mypid || ierr) {
	/* if the traffic came from this program, don't upload it to the server*/
	*ignore_entry = PETSC_TRUE;
	break;
      }
      ierr = process_statistics_add_connlat(pstats,connlat_entry);CHKERRQ(ierr);
      break;
    case TCPLIFE:
      ierr = tcplife_entry_parse_line(life_entry,line);if (ierr) break;
      if ((life_entry)->pid == mypid || ierr) {
	/* if the traffic came from this program, don't upload it to the server */
	*ignore_entry = PETSC_TRUE;
	break;
      }
      ierr = process_statistics_add_life(pstats,life_entry);CHKERRQ(ierr);
      break;
    case TCPRETRANS:
      ierr = tcpretrans_entry_parse_line(retrans_entry,line);if (ierr) break;
      if ((retrans_entry)->pid == mypid || ierr) {
	/* if the traffic came from this program, don't upload it to the server */
	*ignore_entry = PETSC_TRUE;
	break;
      }
      ierr = process_statistics_add_retrans(pstats,retrans_entry);CHKERRQ(ierr);
      break;
    }

  PetscFunctionReturn(0);
}


PetscErrorCode read_file(FILE *fd, size_t *linesize, char **line, PetscInt *nentry, InputType input_type, PetscInt mypid,
			   tcpaccept_entry *accept_entry, tcpconnect_entry *connect_entry,
			   tcpconnlat_entry *connlat_entry, tcplife_entry *life_entry,
			   tcpretrans_entry *retrans_entry, PetscBool *ignore_entry,
			   process_statistics *pstats)
{
  size_t nread;
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  /* skip header lines */
  //getline(line,linesize,fd);
  //getline(line,linesize,fd);
  *nentry = 0;

  while ((nread = getline(line,linesize,fd)) != -1) {
    ierr = handle_line(*line,*nentry,input_type,
		       mypid,accept_entry,connect_entry,
		       connlat_entry,life_entry,retrans_entry,
		       ignore_entry,pstats);CHKERRQ(ierr);
    if (*ignore_entry) {
      PetscFPrintf(PETSC_COMM_WORLD,stderr,"Ignoring entry\n");
      *ignore_entry = PETSC_FALSE;
      continue;
    } else {
      ++(*nentry);
    }
  }
  PetscFPrintf(PETSC_COMM_WORLD,stderr,"Handled %D entries.\n",*nentry);
  PetscFunctionReturn(0);
}

void sigint_handler(int sig_num)
{
  buffer_destroy(&buf);
  if (line) {
    free(line);
  }
  if (input.file) {
    fclose(input.file);
  }
  process_statistics_destroy(&pstats);
  PetscFinalize();
  exit(sig_num);
}

void sigabort_handler(int sig)
{
  void *arr[20];
  size_t sz;

  sz = backtrace(arr,20);
  backtrace_symbols_fd(arr,sz,STDERR_FILENO);
  exit(sig);
}

PetscErrorCode fork_server(MPI_Comm *inter, char *launcher_path, char *server_path, char *server_input_file)
{
  PetscFunctionBeginUser;
  char path[PETSC_MAX_PATH_LEN];
  sprintf(path,"python3 %s -f %s",server_path,server_input_file);
  int spawn_error;
  PetscPrintf(PETSC_COMM_WORLD,"Launching at %s, gonna broadcast %D chars\n",launcher_path,PETSC_MAX_PATH_LEN);
  MPI_Comm_spawn(launcher_path,MPI_ARGV_NULL,1,MPI_INFO_NULL,0,PETSC_COMM_SELF,inter,&spawn_error);
  MPI_Send(path,PETSC_MAX_PATH_LEN,MPI_CHAR,0,0,*inter);
  PetscFunctionReturn(spawn_error);
}

int main(int argc, char **argv)
{
  PetscErrorCode ierr;
  PetscBag       bag;
  size_t         buf_capacity,linesize,nread;
  PetscInt       N,ires,nentry,mypid,rank,size,num_pid,i,*pids;
  PetscReal      polling_interval;
  char           filename[PETSC_MAX_PATH_LEN],url_filename[PETSC_MAX_PATH_LEN],sawsurl[256];
  char           accept_filename[PETSC_MAX_PATH_LEN], connect_filename[PETSC_MAX_PATH_LEN],
                 connlat_filename[PETSC_MAX_PATH_LEN], output_filename[PETSC_MAX_PATH_LEN],
                 life_filename[PETSC_MAX_PATH_LEN], retrans_filename[PETSC_MAX_PATH_LEN],
    python_server_name[PETSC_MAX_PATH_LEN], python_launcher_name[PETSC_MAX_PATH_LEN];
  MPI_Comm       server_comm;
  FILE           *output;
  PetscBool      has_filename,has_filename2,ignore_entry,has_accept,has_connect,has_connlat,has_life,has_retrans,has_input_filename;
  InputType      input_type;
  tcpaccept_entry accept_entry;
  tcpconnect_entry connect_entry;
  tcpconnlat_entry connlat_entry;
  tcplife_entry life_entry;
  tcpretrans_entry retrans_entry;
  PetscViewer     viewer;
  process_data       *pdata;
  process_data_summary *psumm;
  //signal(SIGABRT,sigabort_handler);
  ierr = PetscInitialize(&argc,&argv,NULL,help);if (ierr) return ierr;
  ierr = register_mpi_types();CHKERRQ(ierr);
  mypid = getpid();
  MPI_Comm_rank(PETSC_COMM_WORLD,&rank);
  MPI_Comm_size(PETSC_COMM_WORLD,&size);
  line = NULL;
  linesize = 0;

  has_filename = has_filename2 = ignore_entry = has_accept = has_connect = has_connlat = has_life = has_retrans = has_input_filename = PETSC_FALSE;

  ierr = PetscOptionsGetString(NULL,NULL,"--python_server",python_server_name,PETSC_MAX_PATH_LEN,&has_filename);
  if (!has_filename) {
    strcpy(python_server_name,"petsc_webserver_backend.py");
  }
  ierr = PetscOptionsGetString(NULL,NULL,"--python_launcher",python_launcher_name,PETSC_MAX_PATH_LEN,&has_filename);
  if (!has_filename) {
    strcpy(python_launcher_name,"./webserver_launcher");
  }
  ierr = PetscOptionsGetString(NULL,NULL,"-file",filename,PETSC_MAX_PATH_LEN,&has_input_filename);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(NULL,NULL,"--accept_file",accept_filename,PETSC_MAX_PATH_LEN,&has_accept);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(NULL,NULL,"--connect_file",connect_filename,PETSC_MAX_PATH_LEN,&has_connect);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(NULL,NULL,"--connlat_file",connlat_filename,PETSC_MAX_PATH_LEN,&has_connlat);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(NULL,NULL,"--life_file",life_filename,PETSC_MAX_PATH_LEN,&has_life);CHKERRQ(ierr);
  ierr = PetscOptionsGetString(NULL,NULL,"--retrans_file",retrans_filename,PETSC_MAX_PATH_LEN,&has_retrans);CHKERRQ(ierr);
  if (!(has_input_filename || has_accept || has_connect || has_connlat || has_life || has_retrans)) {
    SETERRQ(PETSC_COMM_WORLD,1,"Must provide an input filename (-file and -type and/or any of --accept_file,--connect_file,etc.)");
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
  ierr = process_statistics_init(&pstats);CHKERRQ(ierr);
  /* read each file */
  if (has_input_filename) {
    input.file = fopen(filename,"r");

    ierr = read_file(input.file,&linesize,&line,&nentry,input_type,mypid,&accept_entry,
		     &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		     &ignore_entry,&pstats);CHKERRQ(ierr);
  } /* if has_input_filename */

  if (has_accept) {
    accept_input.file = fopen(accept_filename,"r");
    ierr = read_file(accept_input.file,&linesize,&line,&nentry,TCPACCEPT,mypid,&accept_entry,
		     &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		     &ignore_entry,&pstats);CHKERRQ(ierr);
  }

  if (has_connect) {
    connect_input.file = fopen(connect_filename,"r");
    ierr = read_file(connect_input.file,&linesize,&line,&nentry,TCPCONNECT,mypid,&accept_entry,
		     &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		     &ignore_entry,&pstats);CHKERRQ(ierr);
  }

  if (has_connlat) {
    connlat_input.file = fopen(connlat_filename,"r");
    ierr = read_file(connlat_input.file,&linesize,&line,&nentry,TCPCONNLAT,mypid,&accept_entry,
		     &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		     &ignore_entry,&pstats);CHKERRQ(ierr);
  }

  if (has_life) {
    life_input.file = fopen(life_filename,"r");
    ierr = read_file(life_input.file,&linesize,&line,&nentry,TCPLIFE,mypid,&accept_entry,
		     &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		     &ignore_entry,&pstats);CHKERRQ(ierr);
  }

  if (has_retrans) {
    retrans_input.file = fopen(retrans_filename,"r");
    ierr = read_file(retrans_input.file,&linesize,&line,&nentry,TCPRETRANS,mypid,&accept_entry,
		     &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		     &ignore_entry,&pstats);CHKERRQ(ierr);
  }
  


  ierr = process_statistics_num_entries(&pstats,&num_pid);CHKERRQ(ierr);
  /* done with input files */
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

  ierr = buffer_gather_summaries(&buf);CHKERRQ(ierr);
  
  if (!rank) {
    while (!buffer_empty(&buf)) {
      ierr = buffer_get_item(&buf,&bag);CHKERRQ(ierr);
      ierr = PetscBagGetData(bag,(void**)&psumm);CHKERRQ(ierr);
      ierr = summary_view(output,psumm);CHKERRQ(ierr);
      ierr = buffer_pop(&buf);CHKERRQ(ierr);
    }
  }
  if (!rank) {
    if (output != stdout && output != stderr) {
      fclose(output);
    }
  }
					    
  MPI_Barrier(PETSC_COMM_WORLD);
  
  if (!rank) {
    // launch server 
    //printf("about to fork\n");
    ierr = fork_server(&server_comm,python_launcher_name,python_server_name,output_filename);CHKERRQ(ierr);
  }
  MPI_Barrier(PETSC_COMM_WORLD);
  /* done with the file; now wait for more data; */
  while (PETSC_TRUE) {
    if (has_input_filename && has_new_data(&input)) {
      ierr = read_file(input.file,&linesize,&line,&nentry,input_type,mypid,
		       &accept_entry, &connect_entry,&connlat_entry,&life_entry,
		       &retrans_entry,&ignore_entry,&pstats);CHKERRQ(ierr);
    }
    if (has_accept && has_new_data(&accept_input))  {
      ierr = read_file(accept_input.file,&linesize,&line,&nentry,TCPACCEPT,mypid,&accept_entry,
		       &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		       &ignore_entry,&pstats);CHKERRQ(ierr);
    }
    if (has_connect && has_new_data(&connect_input)) {
      ierr = read_file(connect_input.file,&linesize,&line,&nentry,TCPCONNECT,mypid,&accept_entry,
		       &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		       &ignore_entry,&pstats);CHKERRQ(ierr);
    }
    if (has_connlat && has_new_data(&connlat_input)) {
      ierr = read_file(connlat_input.file,&linesize,&line,&nentry,TCPCONNLAT,mypid,&accept_entry,
		       &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		       &ignore_entry,&pstats);CHKERRQ(ierr);
    }
    if (has_life && has_new_data(&life_input)) {
      ierr = read_file(life_input.file,&linesize,&line,&nentry,TCPLIFE,mypid,&accept_entry,
		       &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		       &ignore_entry,&pstats);CHKERRQ(ierr);
    }
    if (has_retrans && has_new_data(&retrans_input)) {
      ierr = read_file(retrans_input.file,&linesize,&line,&nentry,TCPRETRANS,mypid,&accept_entry,
		       &connect_entry,&connlat_entry,&life_entry,&retrans_entry,
		       &ignore_entry,&pstats);CHKERRQ(ierr);
    }
    
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
      if (output != stdout && output != stderr) {
	output = fopen(output_filename,"w");
      }
      while (!buffer_empty(&buf)) {
	ierr = buffer_get_item(&buf,&bag);CHKERRQ(ierr);
	ierr = PetscBagGetData(bag,(void**)&psumm);CHKERRQ(ierr);
	ierr = summary_view(output,psumm);CHKERRQ(ierr);
	ierr = buffer_pop(&buf);CHKERRQ(ierr);
      }
      fclose(output);
    }
      
      /* wait for new entries */
    ierr = PetscSleep(polling_interval);CHKERRQ(ierr);
  }
  /* end main event loop */
  //SAWs_Finalize();
  ierr = PetscFree(pids);CHKERRQ(ierr);
  ierr = PetscFree(pdata);CHKERRQ(ierr);
  PetscFinalize();
  return 0;
}
