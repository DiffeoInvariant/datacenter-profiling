#include "petsc_webserver.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <petscsys.h>
#include <limits.h>
#include <petscviewersaws.h>


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
    PetscFPrintf(PETSC_COMM_WORLD,stderr,"Error parsing string %s for ipv4 address.\n",str);
  }
  PetscFunctionReturn(0);
}

PetscErrorCode parse_ipv6(const char *str, char *ip)
{
  PetscFunctionBeginUser;
  const char *format = "%39[0-9:a-z]";
  if (sscanf(str,format,ip) != 1) {
    PetscFPrintf(PETSC_COMM_WORLD,stderr,"Error parsing string %s for ipv6 address.\n",str);
  }
  PetscFunctionReturn(0);
}



#define CHECK_TOKEN(str,tok,i) do {		\
  if (!tok) { \
    PetscFPrintf(PETSC_COMM_WORLD,stderr,"Failed to find expected token number %D in string %s\n",i,str); \
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

PetscErrorCode create_tcpconnlat_entry_bag(tcpconnlat_entry **entryptr, PetscBag *bagptr, PetscInt n)
{
  PetscErrorCode ierr;
  tcpconnlat_entry *entry;
  PetscBag        bag;
  char            obj_name[100];
  PetscFunctionBeginUser;
  ierr = PetscBagCreate(PETSC_COMM_WORLD,sizeof(tcpconnlat_entry),&bag);CHKERRQ(ierr);
  ierr = PetscBagGetData(bag,(void**)&entry);CHKERRQ(ierr);
  sprintf(obj_name,"tcpconnlat_entry_%d",n);
  ierr = PetscBagRegisterInt(bag,&entry->pid,-1,"pid","Process ID that accepted the connection");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->ip,4,"ip","IP address version");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->dport,0,"dport","Remote port");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->saddr,IP_ADDR_MAX_LEN,"0.0.0.0","saddr","Source IP address");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->daddr,IP_ADDR_MAX_LEN,"0.0.0.0","daddr","Destination IP address");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->comm,COMM_MAX_LEN,"[unknown]","comm","Process name");CHKERRQ(ierr);

  *entryptr = entry;
  *bagptr = bag;
  PetscFunctionReturn(0);
}

PetscErrorCode create_tcpretrans_entry_bag(tcpretrans_entry **entryptr, PetscBag *bagptr, PetscInt n)
{
  PetscErrorCode ierr;
  tcpretrans_entry *entry;
  PetscBag        bag;
  char            obj_name[100];
  PetscFunctionBeginUser;
  ierr = PetscBagCreate(PETSC_COMM_WORLD,sizeof(tcpconnlat_entry),&bag);CHKERRQ(ierr);
  ierr = PetscBagGetData(bag,(void**)&entry);CHKERRQ(ierr);
  sprintf(obj_name,"tcpconnlat_entry_%d",n);
  ierr = PetscBagRegisterInt(bag,&entry->pid,-1,"pid","Process ID that accepted the connection");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(bag,&entry->ip,4,"ip","IP address version");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->laddr_port,COMM_MAX_LEN,"0.0.0.0:0","laddr_port","Local IP_address:tcp_port");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->raddr_port,COMM_MAX_LEN,"0.0.0.0:0","raddr_port","Remote IP_address:tcp_port");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(bag,&entry->state,COMM_MAX_LEN,"[unknown]","state","TCP session state");CHKERRQ(ierr);

  *entryptr = entry;
  *bagptr = bag;
  PetscFunctionReturn(0);
}

PetscErrorCode tcpretrans_entry_parse_line(tcpretrans_entry *entry, char *str)
{
  PetscErrorCode ierr;
  char *substr;
  const char sep[2] = " ";
  size_t len;
  PetscFunctionBeginUser;
  SETERRQ(PETSC_COMM_WORLD,1,"Error, tcpretrans_entry_parse_line() not implemented yet");
  substr = strtok(str,sep);
  CHECK_TOKEN(str,substr,0);
  entry->pid = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,1);
  entry->ip = atoi(substr);
  while (entry->ip == 0) {
    /* in case comm has a space */
    substr = strtok(NULL,sep);
    CHECK_TOKEN(str,substr,1);
    entry->ip = atoi(substr);
  }


  strtok(NULL,sep);
  CHECK_TOKEN(str,substr,4);
  ierr = PetscStrlen(substr,&len);CHKERRQ(ierr);
  ierr = PetscStrncpy(entry->state,substr,len+1);CHKERRQ(ierr);


  PetscFunctionReturn(0);
}

PetscErrorCode tcpconnlat_entry_parse_line(tcpconnlat_entry *entry, char *str)
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
  CHECK_TOKEN(str,substr,6);
  entry->dport = atoi(substr);
  substr = strtok(NULL,sep);
  CHECK_TOKEN(str,substr,7);
  entry->lat_ms = atof(substr);
  PetscFunctionReturn(0);
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
  PetscStrncpy(pdata.comm,entry->comm,COMM_MAX_LEN);
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
  PetscStrncpy(pdata.comm,entry->comm,COMM_MAX_LEN);
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
  PetscStrncpy(pdata.comm,entry->comm,COMM_MAX_LEN);
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
  PetscStrncpy(pdata.comm,entry->comm,COMM_MAX_LEN);
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
  PetscStrncpy(pdata.comm,"[unknown]",sizeof("[unknown]"));
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


  MPI_Aint pdata_displacements[] = {offsetof(process_data_summary,pid),
				    offsetof(process_data_summary,rank),
				    offsetof(process_data_summary,tx_kb),
				    offsetof(process_data_summary,rx_kb),
				    offsetof(process_data_summary,n_event),
				    offsetof(process_data_summary,avg_latency),
				    //offsetof(process_data_summary,sd_latency),
				    offsetof(process_data_summary,avg_lifetime),
				    //offsetof(process_data_summary,sd_lifetime),
				    offsetof(process_data_summary,fraction_ipv6),
				    offsetof(process_data_summary,comm)};

  MPI_Datatype pdata_dtypes[] = {MPI_INT,MPI_INT,MPI_LONG,MPI_LONG,MPI_LONG,
				 MPI_DOUBLE,MPI_DOUBLE,MPI_DOUBLE,MPI_CHAR};

  int pdata_block_lens[] = {1,1,1,1,1,1,1,1,COMM_MAX_LEN};

  MPI_Type_create_struct(9,pdata_block_lens,pdata_displacements,pdata_dtypes,
			 &MPI_DTYPES[DTYPE_SUMMARY]);

  PetscInt i;
  for (i=0; i<6; ++i) {
    MPI_Type_commit(&MPI_DTYPES[i]);
  }

  registered = PETSC_TRUE;
  PetscFunctionReturn(0);
}


PetscErrorCode process_data_summarize(PetscInt pid, process_data *pdata, process_data_summary *psumm)
{
  PetscFunctionBeginUser;
  psumm->pid = pid;
  psumm->tx_kb = pdata->tx_kb;
  psumm->rx_kb = pdata->rx_kb;
  psumm->n_event = pdata->naccept + pdata->nconnect + pdata->nconnlat
    + pdata->nlife + pdata->nretrans;
  psumm->avg_latency = pdata->latms / pdata->nconnlat;
  psumm->avg_lifetime = pdata->lifems / pdata->nlife;
  psumm->fraction_ipv6 = ((PetscReal)(pdata->nipv6))/ ((PetscReal)(pdata->nipv6) + (PetscReal)(pdata->nipv4));
  PetscStrncpy(psumm->comm,pdata->comm,COMM_MAX_LEN);
  PetscFunctionReturn(0);
}


PetscErrorCode process_statistics_get_summary(process_statistics *pstats, PetscInt pid, process_data_summary *psumm)
{
  process_data pdata;
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataGet(pstats->ht,pid,&pdata);CHKERRQ(ierr);
  ierr = process_data_summarize(pid,&pdata,psumm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode process_statistics_get_pid_data(process_statistics *pstats,
					       PetscInt pid,
					       process_data *pdata)
{
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataGet(pstats->ht,pid,pdata);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


PetscErrorCode process_statistics_num_entries(process_statistics *pstats, PetscInt *n)
{
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataGetSize(pstats->ht,n);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}




  
PetscErrorCode process_statistics_get_all(process_statistics *pstats, process_data *pdatas, PetscInt *pids)
{
  PetscErrorCode ierr;
  PetscInt off=0;
  PetscFunctionBeginUser;
  ierr = PetscHMapDataGetPairs(pstats->ht,&off,pids,pdatas);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
    


PetscErrorCode create_process_summary_bag(process_data_summary **psumm, PetscBag *bag, PetscInt rank, PetscInt nentry)
{
  PetscErrorCode ierr;
  process_data_summary *ps;
  PetscBag pbag;
  char obj_name[100];
  PetscFunctionBeginUser;
  sprintf(obj_name,"process_data_summary_rank_%d_n_%d",rank,nentry);
  ierr = PetscBagCreate(PETSC_COMM_WORLD,sizeof(process_data_summary),&pbag);CHKERRQ(ierr);
  ierr = PetscBagGetData(pbag,(void**)&ps);CHKERRQ(ierr);
  ierr = PetscBagSetName(pbag,obj_name,"A process summary");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(pbag,&ps->pid,-1,"pid","Process ID");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt(pbag,&ps->rank,0,"rank","MPI rank");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt64(pbag,&ps->tx_kb,0,"tx_kb","Transmitted kilobytes");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt64(pbag,&ps->rx_kb,0,"rx_kb","Received kilobytes");CHKERRQ(ierr);
  ierr = PetscBagRegisterInt64(pbag,&ps->n_event,0,"n_event","Number of TCP events (e.g. connect, accept, life, etc.)");CHKERRQ(ierr);
  ierr = PetscBagRegisterReal(pbag,&ps->avg_latency,0.0,"avg_latency","Average connection latency from tcpconnlat");CHKERRQ(ierr);
  //ierr = PetscBagRegisterReal(pbag,&ps->sd_latency,0.0,"sd_latency","Standard deviation of connection latency from tcpconnlat");CHKERRQ(ierr);
  ierr = PetscBagRegisterReal(pbag,&ps->avg_lifetime,0.0,"avg_lifetime","Average lifetime of a TCP event from tcplife");CHKERRQ(ierr);
  //ierr = PetscBagRegisterReal(pbag,&ps->sd_lifetime,0.0,"sd_lifetime","Standard deviation of the lifetimes of TCP events from tcplife");CHKERRQ(ierr);
  ierr = PetscBagRegisterReal(pbag,&ps->fraction_ipv6,0.0,"fraction_ipv6","Fraction of the TCP events that used IP v6 instead of v4");CHKERRQ(ierr);
  ierr = PetscBagRegisterString(pbag,&ps->comm,COMM_MAX_LEN,"[unknown]","comm","Process name");CHKERRQ(ierr);

  *bag = pbag;
  *psumm = ps;
  PetscFunctionReturn(0);
}


PetscErrorCode buffer_gather_summaries(entry_buffer *buf)
{
  PetscErrorCode ierr;
  PetscFunctionBeginUser;
  ierr = buffer_gather(buf,DTYPE_SUMMARY);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


PetscErrorCode buffer_gather(entry_buffer *buf, SERVER_MPI_DTYPE dtype)
{
  PetscErrorCode ierr;
  PetscInt       rank,size,nitem,nextant,i,ires;
  process_data_summary *summaries=NULL,*summary,*ssummaries=NULL;
  PetscBag             bag;
  PetscFunctionBeginUser;
  MPI_Comm_rank(PETSC_COMM_WORLD,&rank);
  MPI_Comm_size(PETSC_COMM_WORLD,&size);
  PetscInt summs;
  if (size == 1) {
    /* only one process in the communicator; already on root */
    PetscFunctionReturn(0);
  }
  /* prepare for MPI_Reduce() to get number of summaries coming into root */
  if (rank) {
    summs = 0;
    if (buf->num_items > INT_MAX) {
      SETERRQ2(PETSC_COMM_WORLD,1,"Number of items %D on rank %D is greater than INT_MAX! Try sending the buffer with fewer items.",buf->num_items,rank);
    }
    nitem = (PetscInt)buf->num_items;
    ierr = PetscCalloc1(nitem,&ssummaries);CHKERRQ(ierr);
    /* fill the array with summaries */
    i=0;
    while (!buffer_empty(buf)) {
      ierr = buffer_get_item(buf,&bag);CHKERRQ(ierr);
      ierr = PetscBagGetData(bag,(void**)&summary);CHKERRQ(ierr);
      ssummaries[i] = *summary;   
      ierr = buffer_pop(buf);CHKERRQ(ierr);
      ++i;
    }
  } else {
    nitem = 0;
    if (buf->num_items > INT_MAX) {
      SETERRQ2(PETSC_COMM_WORLD,1,"Number of items %D on rank %D is greater than INT_MAX! Try sending the buffer with fewer items.",buf->num_items,rank);
    }
    //ierr = PetscCalloc1(size,&summs_per_rank);CHKERRQ(ierr);
    nextant = (PetscInt)buf->num_items;
  }
  MPI_Barrier(PETSC_COMM_WORLD);
  MPI_Reduce(&nitem,&summs,1,MPI_INT,MPI_SUM,0,PETSC_COMM_WORLD);
  //MPI_Gather(&nitem,1,MPI_INT,summs_per_rank,1,MPI_INT,0,PETSC_COMM_WORLD);

  if (!rank) {
    if (nextant + summs >= buf->capacity) {
      SETERRQ3(PETSC_COMM_WORLD,1,"Buffer on root has capacity %D, but currently holds %D entries and is being asked to accept %D more! Increase the buffer size on root.",buf->capacity,nextant,nitem);
    }
    ierr = PetscCalloc1(summs+nextant,&summaries);CHKERRQ(ierr);
  }

  MPI_Gather(ssummaries,nitem,MPI_DTYPES[dtype],
	     summaries,summs,MPI_DTYPES[dtype],
	     0,PETSC_COMM_WORLD);

  /* push the new summaries into the buffer */
  if (!rank) {
    for (i=0; i<summs; ++i) {
      ierr = create_process_summary_bag(&summary,&bag,summaries[i].rank,
					i);
      //*summary = summaries[i];
      summary->pid = summaries[i].pid;
      summary->rank = summaries[i].rank;
      summary->tx_kb = summaries[i].tx_kb;
      summary->rx_kb = summaries[i].rx_kb;
      summary->n_event = summaries[i].n_event;
      summary->avg_latency = summaries[i].avg_latency;
      summary->fraction_ipv6 = summaries[i].fraction_ipv6;
      ierr = PetscStrncpy(summary->comm,summaries[i].comm,COMM_MAX_LEN);CHKERRQ(ierr);
      
      ires = buffer_try_insert(buf,bag);
      if (ires == -1) {
	PetscPrintf(PETSC_COMM_WORLD,"Error: buffer is full! Try increasing the capacity. Discarding this entry\n.");
      }
      
    }
  } 
  if (rank) {
    ierr = PetscFree(ssummaries);CHKERRQ(ierr);
  } else {
    ierr = PetscFree(summaries);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

   
PetscErrorCode summary_view(FILE *fd, process_data_summary *psum)
{
  PetscFunctionBeginUser;
  PetscFPrintf(PETSC_COMM_WORLD,fd,"Summary of network traffic on rank %D, process %D:\n",psum->rank,psum->pid);
  PetscFPrintf(PETSC_COMM_WORLD,fd,"pid           = %D\n",psum->pid);
  PetscFPrintf(PETSC_COMM_WORLD,fd,"name          = %s\n",psum->comm);
  PetscFPrintf(PETSC_COMM_WORLD,fd,"tx_kb         = %D\n",psum->tx_kb);
  PetscFPrintf(PETSC_COMM_WORLD,fd,"rx_kb         = %D\n",psum->rx_kb);
  PetscFPrintf(PETSC_COMM_WORLD,fd,"n_event       = %D\n",psum->n_event);
  PetscFPrintf(PETSC_COMM_WORLD,fd,"avg_latency   = %g\n",psum->avg_latency);
  PetscFPrintf(PETSC_COMM_WORLD,fd,"avg_lifetime  = %g\n",psum->avg_lifetime);
  PetscFPrintf(PETSC_COMM_WORLD,fd,"fraction_ipv6 = %.3g\n",psum->fraction_ipv6);
  PetscFunctionReturn(0);
}
