#include "petsc_webserver.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <petscsys.h>


PetscErrorCode create_tcpaccept_entry_bag(tcpaccept_entry **entryptr, PetscBag *bagptr, PetscInt n)
{
  PetscErrorCode ierr;
  tcpaccept_entry *entry;
  PetscBag        bag;
  char            obj_name[100];
  PetscFunctionBeginUser;
  ierr = PetscBagCreate(PETSC_COMM_WORLD,sizeof(tcpaccept_entry),&bag);CHKERRQ(ierr);
  ierr = PetscBagGetData(bag,(void**)&entry);CHKERRQ(ierr);
  sprintf(obj_name,"tcpaccept_entry_%d",n);
  ierr = PetscBagSetName(bag,obj_name,"An entry generated by the tcpaccept program");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->pid,-1,"pid","Process ID that accepted the connection");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->ip,4,"ip","IP address version");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->rport,0,"rport","Remote port");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->lport,0,"lport","Local port");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->laddr,IP_ADDR_MAX_LEN,"0.0.0.0","laddr","Local IP address");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->raddr,IP_ADDR_MAX_LEN,"0.0.0.0","raddr","Remote IP address");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->comm,COMM_MAX_LEN,"[unknown]","comm","Process name");CHKERRQ(ierr);
  
  *entryptr = entry;
  *bagptr = bag;
  PetscFunctionReturn(0);
}



PetscErrorCode buffer_create(entry_buffer *buf, size_t num_items)
{
  PetscFunctionBeginUser;
  buf->capacity = num_items;
  if (num_items < 2) {
    SETERRQ1(PETSC_COMM_WORLD,1,"Must allocate at least 2 slots in the buffer, not %D",num_items);
  }
  PetscCalloc1(num_items,&buf->buf);
  
  buf->valid_start = 0;
  buf->valid_end = 0;
  buf->num_items = 0;
  PetscFunctionReturn(0);
}

PetscErrorCode buffer_destroy(entry_buffer *buf)
{
  PetscInt i;
  for (i=buf->valid_start; i<buf->valid_end; ++i) {
    if (buf->buf[i]) {
      PetscBagDestroy(&(buf->buf[i]));
    }
  }
  PetscFree(buf->buf);
  buf->valid_start = 0;
  buf->valid_end = 0;
  buf->capacity = 0;
  buf->num_items = 0;
  return 0;
}

PetscBool buffer_full(entry_buffer *buf)
{
  return (buf->num_items == buf->capacity);
}

PetscBool buffer_empty(entry_buffer *buf)
{
  return (buf->num_items == 0);
}


size_t buffer_capacity(entry_buffer *buf)
{
  return buf->capacity;
}


size_t buffer_size(entry_buffer *buf)
{
  if (!buf) {
    SETERRQ(PETSC_COMM_WORLD,PETSC_ERR_ARG_NULL,"Null buffer passed to buffer_size()!");
  }
  if (buffer_full(buf)) {
    return buf->capacity;
  }
  return buf->num_items;
}


PetscInt buffer_try_insert(entry_buffer *buf, PetscBag dataptr)
{
  if (!buf) {
    SETERRQ(PETSC_COMM_WORLD,PETSC_ERR_ARG_NULL,"Null buffer passed to buffer_try_insert()!");
  }
  if (buffer_full(buf)) {
    return -1;
  }
  
  buf->buf[buf->valid_end] = dataptr;
  
  buf->valid_end = (buf->valid_end + 1) % buf->capacity;
  ++(buf->num_items);
  return 0;
}

PetscErrorCode buffer_get_item(entry_buffer *buf, PetscBag *itemptr)
{
  if (buffer_empty(buf)) {
    return 0;
  }
  *itemptr = buf->buf[buf->valid_start];
  return 0;
}

PetscErrorCode buffer_pop(entry_buffer *buf)
{
  PetscBag bag;
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  /* destroy existing entry */
  ierr = buffer_get_item(buf,&bag);CHKERRQ(ierr);
  if (bag) {
    ierr = PetscBagDestroy(&bag);CHKERRQ(ierr);
  }
  buf->valid_start = (buf->valid_start + 1) % buf->capacity;
  --(buf->num_items);
  PetscFunctionReturn(0);
}
  
long get_file_end_offset(file_wrapper *file)
{
  long offset;

  fseek(file->file,0,SEEK_END);
  offset = ftell(file->file);
  file->offset = offset;
  return offset;
}


long has_new_data(file_wrapper *file)
{
  long old_offset, new_offset;
  old_offset = file->offset;
  new_offset = get_file_end_offset(file);
  fseek(file->file,old_offset,SEEK_SET);
  return new_offset - old_offset;
}

  
int next_occurance(const char *str, int curr_pos, char ch)
{
  int i = curr_pos;
  for (; str[i]; ++i) {
    if (str[i] == ch) {
      return i;
    }
  }
  return i;
}

int next_nonoccurance(const char *str, int curr_pos, char ch)
{
  int i = curr_pos;
  for (; str[i]; ++i) {
    if (str[i] != ch) {
      return i;
    }
  }
  return i;
}

int next_whitespace(const char *str, int curr_pos)
{
  return next_occurance(str,curr_pos,' ');
}

int next_nonwhitespace(const char *str, int curr_pos)
{
  return next_nonoccurance(str,curr_pos,' ');
}

PetscErrorCode parse_ipv4(const char *str, char *ip)
{
  PetscFunctionBeginUser;
  const char *format = "%15[0-9.]";
  if (sscanf(str,format,ip) != 1) {
    SETERRQ1(PETSC_COMM_WORLD,1,"Error parsing string %s for ipv4 address.",str);
  }
  PetscFunctionReturn(0);
}

PetscErrorCode parse_ipv6(const char *str, char *ip)
{
  PetscFunctionBeginUser;
  const char *format = "%39[0-9:a-z]";
  if (sscanf(str,format,ip) != 1) {
    SETERRQ1(PETSC_COMM_WORLD,1,"Error parsing string %s for ipv6 address.",str);
  }
  PetscFunctionReturn(0);
}



#define CHECK_TOKEN(str,tok,i) do {		\
  if (!tok) { \
  PetscPrintf(PETSC_COMM_WORLD,"Failed to find expected token number %D in string %s\n",i,str); \
  return i; \
  }\
  } while(0)



PetscErrorCode tcpaccept_entry_parse_line(tcpaccept_entry *entry, char *str)
{
  PetscErrorCode ierr;
  char *substr;
  const char sep[2] = " ";
  size_t len;
  PetscFunctionBeginUser;
  substr = strtok(str,sep);
  CHECK_TOKEN(str,substr,0);
  entry->pid = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,1);
  ierr = PetscStrlen(substr,&len);CHKERRQ(ierr);
  ierr = PetscStrncpy(entry->comm,substr,len+1);CHKERRQ(ierr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,2);
  entry->ip = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,3);
  if (entry->ip == 4) {
    ierr = parse_ipv4(substr,entry->raddr);CHKERRQ(ierr);
  } else {
    ierr = parse_ipv6(substr,entry->raddr);CHKERRQ(ierr);
  }
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,4);
  entry->rport = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,5);
  if (entry->ip == 4) {
    ierr = parse_ipv4(substr,entry->laddr);CHKERRQ(ierr);
  } else {
    ierr = parse_ipv6(substr,entry->laddr);CHKERRQ(ierr);
  }
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,6);
  entry->lport = atoi(substr);
  PetscFunctionReturn(0);
}
  
PetscErrorCode tcpconnect_entry_parse_line(tcpconnect_entry *entry, char *str)
{
  PetscErrorCode ierr;
  char *substr;
  const char sep[2] = " ";
  size_t len;
  PetscFunctionBeginUser;
  substr = strtok(str,sep);
  CHECK_TOKEN(str,substr,0);
  entry->pid = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,1);
  ierr = PetscStrlen(substr,&len);CHKERRQ(ierr);
  ierr = PetscStrncpy(entry->comm,substr,len+1);CHKERRQ(ierr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,2);
  entry->ip = atoi(substr);
  while (entry->ip == 0) {
    /* in case comm has a space */
    substr = strtok(NULL,sep);
    CHECK_TOKEN(str,substr,2);
    entry->ip = atoi(substr);
  }
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,3);
  if (entry->ip == 4) {
    ierr = parse_ipv4(substr,entry->saddr);CHKERRQ(ierr);
  } else {
    ierr = parse_ipv6(substr,entry->saddr);CHKERRQ(ierr);
  }
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,4);
  if (entry->ip == 4) {
    ierr = parse_ipv4(substr,entry->daddr);CHKERRQ(ierr);
  } else {
    ierr = parse_ipv6(substr,entry->daddr);CHKERRQ(ierr);
  }
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,5);
  entry->dport = atoi(substr);
  PetscFunctionReturn(0);
}


PetscErrorCode create_tcpconnect_entry_bag(tcpconnect_entry **entryptr, PetscBag *bagptr, PetscInt n)
{
  PetscErrorCode ierr;
  tcpconnect_entry *entry;
  PetscBag        bag;
  char            obj_name[100];
  PetscFunctionBeginUser;
  ierr = PetscBagCreate(PETSC_COMM_WORLD,sizeof(tcpconnect_entry),&bag);CHKERRQ(ierr);
  ierr = PetscBagGetData(bag,(void**)&entry);CHKERRQ(ierr);
  sprintf(obj_name,"tcpconnect_entry_%d",n);
  ierr = PetscBagSetName(bag,obj_name,"An entry generated by the tcpconnect program");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->pid,-1,"pid","Process ID that requested the connection");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->ip,4,"ip","IP address version");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->dport,0,"dport","Destination port");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->saddr,IP_ADDR_MAX_LEN,"0.0.0.0","saddr","Source IP address");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->daddr,IP_ADDR_MAX_LEN,"0.0.0.0","daddr","Destination IP address");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->comm,COMM_MAX_LEN,"[unknown]","comm","Process name");CHKERRQ(ierr);
  
  *entryptr = entry;
  *bagptr = bag;
  PetscFunctionReturn(0);
}


PetscErrorCode create_tcplife_entry_bag(tcplife_entry **entryptr, PetscBag *bagptr, PetscInt n)
{
  PetscErrorCode ierr;
  tcplife_entry *entry;
  PetscBag        bag;
  char            obj_name[100];
  PetscFunctionBeginUser;
  ierr = PetscBagCreate(PETSC_COMM_WORLD,sizeof(tcplife_entry),&bag);CHKERRQ(ierr);
  ierr = PetscBagGetData(bag,(void**)&entry);CHKERRQ(ierr);
  sprintf(obj_name,"tcplife_entry_%d",n);
  ierr = PetscBagSetName(bag,obj_name,"An entry generated by the tcplife program");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->pid,-1,"pid","Process ID that requested the connection");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->ip,4,"ip","IP address version");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->lport,0,"lport","Local port");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->rport,0,"rport","Remote port");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->tx_kb,0,"tx_kb","Transmitted kB");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->rx_kb,0,"rx_kb","Received kB");CHKERRQ(ierr);
  ierr = PetscBagRegisterReal(bag,&entry->ms,0.,"ms","Milliseconds");
  ierr = PetscBagRegisterString(bag,&entry->laddr,IP_ADDR_MAX_LEN,"0.0.0.0","laddr","Local IP address");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->raddr,IP_ADDR_MAX_LEN,"0.0.0.0","raddr","Remote IP address");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->comm,COMM_MAX_LEN,"[unknown]","comm","Process name");CHKERRQ(ierr);
  
  *entryptr = entry;
  *bagptr = bag;
  PetscFunctionReturn(0);
}


PetscErrorCode tcplife_entry_parse_line(tcplife_entry *entry, char *str)
{
  PetscErrorCode ierr;
  char *substr;
  const char sep[2] = ",";
  size_t len;
  PetscFunctionBeginUser;
  substr = strtok(str,sep);
  CHECK_TOKEN(str,substr,0);
  entry->pid = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,1);
  ierr = PetscStrlen(substr,&len);CHKERRQ(ierr);
  ierr = PetscStrncpy(entry->comm,substr,len+1);CHKERRQ(ierr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,2);
  entry->ip = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,3);
  if (entry->ip == 4) {
    ierr = parse_ipv4(substr,entry->laddr);CHKERRQ(ierr);
  } else {
    ierr = parse_ipv6(substr,entry->laddr);CHKERRQ(ierr);
  }
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,4);
  entry->lport = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,5);
  if (entry->ip == 4) {
    ierr = parse_ipv4(substr,entry->raddr);CHKERRQ(ierr);
  } else {
    ierr = parse_ipv6(substr,entry->raddr);CHKERRQ(ierr);
  }
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,6);
  entry->rport = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,7);
  entry->tx_kb = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,8);
  entry->rx_kb = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,9);
  entry->ms = atof(substr);
  PetscFunctionReturn(0);
}

PetscErrorCode create_tcpconnlat_entry_bag(tcpconnlat_entry **e, PetscBag *b, PetscInt n)
{
  return 0;
}

PetscErrorCode create_tcpretrans_entry_bag(tcpretrans_entry **e, PetscBag *b)
{
  return 0;
}

PetscErrorCode tcpretrans_entry_parse_line(tcpretrans_entry *e, char *b)
{
  return 0;
}

PetscErrorCode tcpconnlat_entry_parse_line(tcpconnlat_entry *e, char *b)
{
  return 0;
}




PetscErrorCode process_data_initialize(process_data *dat)
{
  PetscMemzero(dat,sizeof(process_data));
  return 0;
}






PetscErrorCode process_statistics_init(process_statistics *pstats)
{
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataCreate(&(pstats->ht));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode process_statistics_destroy(process_statistics *pstats)
{
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataDestroy(&(pstats->ht));CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode process_statistics_add_accept(process_statistics *pstats,
					     tcpaccept_entry *entry)
{
  PetscErrorCode ierr;
  process_data   pdata;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataGet(pstats->ht,entry->pid,&pdata);CHKERRQ(ierr);
  ++pdata.naccept;
  if (entry->ip == 4) {
    ++pdata.nipv4;
  } else if (entry->ip == 6) {
    ++pdata.nipv6;
  }
  ierr = PetscHMapDataSet(pstats->ht,entry->pid,pdata);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


PetscErrorCode process_statistics_add_connect(process_statistics *pstats,
					      tcpconnect_entry *entry)
{
  PetscErrorCode ierr;
  process_data   pdata;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataGet(pstats->ht,entry->pid,&pdata);CHKERRQ(ierr);
  ++pdata.nconnect;
  if (entry->ip == 4) {
    ++pdata.nipv4;
  } else if (entry->ip == 6) {
    ++pdata.nipv6;
  }
  ierr = PetscHMapDataSet(pstats->ht,entry->pid,pdata);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


PetscErrorCode process_statistics_add_connlat(process_statistics *pstats,
					      tcpconnlat_entry *entry)
{
  PetscErrorCode ierr;
  process_data   pdata;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataGet(pstats->ht,entry->pid,&pdata);CHKERRQ(ierr);
  ++pdata.nconnlat;
  pdata.latms += entry->lat_ms;
  if (entry->ip == 4) {
    ++pdata.nipv4;
  } else if (entry->ip == 6) {
    ++pdata.nipv6;
  }
  ierr = PetscHMapDataSet(pstats->ht,entry->pid,pdata);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode process_statistics_add_life(process_statistics *pstats,
					   tcplife_entry *entry)
{
  PetscErrorCode ierr;
  process_data   pdata;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataGet(pstats->ht,entry->pid,&pdata);CHKERRQ(ierr);
  ++pdata.nlife;
  pdata.tx_kb += entry->tx_kb;
  pdata.rx_kb += entry->rx_kb;
  pdata.lifems += entry->ms;
  if (entry->ip == 4) {
    ++pdata.nipv4;
  } else if (entry->ip == 6) {
    ++pdata.nipv6;
  }
  ierr = PetscHMapDataSet(pstats->ht,entry->pid,pdata);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


PetscErrorCode process_statistics_add_retrans(process_statistics *pstats,
					      tcpretrans_entry *entry)
{
  PetscErrorCode ierr;
  process_data   pdata;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataGet(pstats->ht,entry->pid,&pdata);CHKERRQ(ierr);
  ++pdata.nretrans;
  if (entry->ip == 4) {
    ++pdata.nipv4;
  } else if (entry->ip == 6) {
    ++pdata.nipv6;
  }
  ierr = PetscHMapDataSet(pstats->ht,entry->pid,pdata);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


static PetscBool registered = PETSC_FALSE;
PetscErrorCode register_mpi_types()
{
  PetscFunctionBeginUser;
  if (registered) {
    PetscFunctionReturn(0);
  }
  MPI_Aint accept_displacements[] = {offsetof(tcpaccept_entry,pid),
				     offsetof(tcpaccept_entry,ip),
				     offsetof(tcpaccept_entry,rport),
				     offsetof(tcpaccept_entry,lport),
				     offsetof(tcpaccept_entry,laddr),
				     offsetof(tcpaccept_entry,raddr),
				     offsetof(tcpaccept_entry,comm)};
  MPI_Datatype accept_dtypes[] = {MPI_INT,MPI_INT,MPI_INT,MPI_INT,MPI_CHAR,
				  MPI_CHAR,MPI_CHAR};


  int accept_block_lens[] = {1,1,1,1,IP_ADDR_MAX_LEN,IP_ADDR_MAX_LEN,COMM_MAX_LEN};

  MPI_Type_create_struct(7,accept_block_lens,accept_displacements,accept_dtypes,
			 &MPI_DTYPES[DTYPE_ACCEPT]);

  MPI_Aint connect_displacements[] = {offsetof(tcpconnect_entry,pid),
				      offsetof(tcpconnect_entry,ip),
				      offsetof(tcpconnect_entry,dport),
				      offsetof(tcpconnect_entry,saddr),
				      offsetof(tcpconnect_entry,daddr),
				      offsetof(tcpconnect_entry,comm)};

  MPI_Datatype connect_dtypes[] = {MPI_INT,MPI_INT,MPI_INT,MPI_CHAR,MPI_CHAR,
				   MPI_CHAR};

  int connect_block_lens[] = {1,1,1,IP_ADDR_MAX_LEN,IP_ADDR_MAX_LEN,
			      COMM_MAX_LEN};

  MPI_Type_create_struct(6,connect_block_lens,connect_displacements,connect_dtypes,
			 &MPI_DTYPES[DTYPE_CONNECT]);
  

  MPI_Aint connlat_displacements[] = {offsetof(tcpconnlat_entry,pid),
				      offsetof(tcpconnlat_entry,ip),
				      offsetof(tcpconnlat_entry,dport),
				      offsetof(tcpconnlat_entry,lat_ms),
				      offsetof(tcpconnlat_entry,saddr),
				      offsetof(tcpconnlat_entry,daddr),
				      offsetof(tcpconnlat_entry,comm)};

  MPI_Datatype connlat_dtypes[] = {MPI_INT,MPI_INT,MPI_INT,
				   MPI_DOUBLE,MPI_CHAR,MPI_CHAR,MPI_CHAR};

  int connlat_block_lens[] = {1,1,1,1,IP_ADDR_MAX_LEN,IP_ADDR_MAX_LEN,
			      COMM_MAX_LEN};

  MPI_Type_create_struct(7,connlat_block_lens,connlat_displacements,
			 connlat_dtypes,&MPI_DTYPES[DTYPE_CONNLAT]);

  MPI_Aint life_displacements[] = {offsetof(tcplife_entry,pid),
				   offsetof(tcplife_entry,ip),
				   offsetof(tcplife_entry,lport),
				   offsetof(tcplife_entry,rport),
				   offsetof(tcplife_entry,tx_kb),
				   offsetof(tcplife_entry,rx_kb),
				   offsetof(tcplife_entry,ms),
				   offsetof(tcplife_entry,laddr),
				   offsetof(tcplife_entry,raddr),
				   offsetof(tcplife_entry,comm),
				   offsetof(tcplife_entry,time)};
  MPI_Datatype life_dtypes[] = {MPI_INT,MPI_INT,MPI_INT,MPI_INT,MPI_INT,MPI_INT,
				MPI_DOUBLE,MPI_CHAR,MPI_CHAR,MPI_CHAR,MPI_CHAR};

  int life_block_lens[] = {1,1,1,1,1,1,1,IP_ADDR_MAX_LEN,IP_ADDR_MAX_LEN,
			   COMM_MAX_LEN,TIME_LEN};

  MPI_Type_create_struct(11,life_block_lens,life_displacements,life_dtypes,
			 &MPI_DTYPES[DTYPE_LIFE]);

  MPI_Aint retrans_displacements[] = {offsetof(tcpretrans_entry,pid),
				      offsetof(tcpretrans_entry,ip),
				      offsetof(tcpretrans_entry,laddr_port),
				      offsetof(tcpretrans_entry,raddr_port),
				      offsetof(tcpretrans_entry,state)};
  
  MPI_Datatype retrans_dtypes[] = {MPI_INT,MPI_INT,MPI_CHAR,MPI_CHAR,MPI_CHAR};
  int retrans_block_lens[] = {1,1,COMM_MAX_LEN,COMM_MAX_LEN,COMM_MAX_LEN};

  MPI_Type_create_struct(5,retrans_block_lens,retrans_displacements,
			 retrans_dtypes,&MPI_DTYPES[DTYPE_RETRANS]);

  registered = PETSC_TRUE;
  PetscFunctionReturn(0);
}
