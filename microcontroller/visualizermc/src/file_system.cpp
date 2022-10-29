#include "file_system.hpp"
#include "ByteIterator.hpp"

#include "debug.hpp"

#include "algorithm"

#include "async.hpp"

#define FILESYSTEM_HEADERCODE 0x1632


void file_system::_tmpdata_new(size_t memsize){
  if(_tmpdata)
    free(_tmpdata);
  
  _tmpdatasize = memsize;
  _tmpdata = (char*)malloc(memsize);
}

void file_system::_tmpdata_free(){
  if(_tmpdata)
    free(_tmpdata);
  
  _tmpdata = NULL;
}

void file_system::_check_queue(){
  DEBUG_PRINT("notready %s, qesize %d\n", _notready? "true": "false", queue_edit.size());
  if(!_notready && queue_edit.size()){
    DEBUG_PRINT("on donewr\n");
    _notready = true;
    _on_donewr();
  }
}

void file_system::_on_donewr(){
  if(queue_edit.size()){
    uint8_t _qtype = queue_edit[0][0];
    DEBUG_PRINT("qtype: %d\n", (int)_qtype);
    file_info *_fi;
    switch((_queue_enum)_qtype){
      break; case done_r:{
        DEBUG_PRINT("\tstate: done_r\n");
        // calling to specified callback
        read_queue *_rq = reinterpret_cast<read_queue*>(queue_edit[0]);

        if(_rq->cb)
          _rq->cb(_rq->obj);

        _rq->_q = _queue_enum::done;
        _on_donewr();
      }

      break; case done_w:{
        DEBUG_PRINT("\tstate: done_w\n");

        write_queue *_wq = reinterpret_cast<write_queue*>(queue_edit[0]);
        free(_wq->data);

        _wq->_q = _queue_enum::done;
        _on_donewr();
      }
      
      break; case done:{
        DEBUG_PRINT("\tstate: done\n");
        // free the queue, end then erase it from the queue
        free(queue_edit[0]);
        queue_edit.erase(queue_edit.begin());

        _tmpdata_free();

        // if the queue still not empty, calling this function
        if(queue_edit.size())
          _on_donewr();
        else
          _finishwr();
      }

      break; case read:{
        DEBUG_PRINT("\tstate: read\n");
        // just read to specific address
        read_queue *_rq = reinterpret_cast<read_queue*>(queue_edit[0]);
        _fi = _get_fileinfo(_rq->id);

        bound_storage->bufReadAsync(_fi->low_bound, _rq->data, _fi->high_bound-_fi->low_bound, __on_donewr, this);
        _rq->_q = done_r;
      }

      break; case write:{
        DEBUG_PRINT("\tstate: write\n");
        write_queue *_wq = reinterpret_cast<write_queue*>(queue_edit[0]);
        _fi = _get_fileinfo(_wq->id);

        // if datasize is higher than high_bound-low_bound, then reallocate the address
        // if not, just reuse the address
        if(_wq->datasize > (_fi->high_bound-_fi->low_bound)){
          DEBUG_PRINT("reallocate\n");
          if(_bottom_size() < _wq->datasize){
            defrag_queue *dq = (defrag_queue*)malloc(sizeof(defrag_queue));
            *dq = defrag_queue{};
          
            // setting the bounds to zero, excluded when defraging
            _fi->high_bound = 0;
            _fi->low_bound = 0;

            queue_edit.insert(queue_edit.begin(), reinterpret_cast<char*>(dq));
            _on_donewr();
            break;
          }

          _fi->high_bound = _lowest_address;
          _lowest_address -= _wq->datasize;
        }

        DEBUG_PRINT("_hb %d\n", _fi->high_bound);
        DEBUG_PRINT("_lowest_address %d\n", _lowest_address);

        _fi->low_bound = _fi->high_bound-_wq->datasize;

        // writing to storage
        bound_storage->bufWriteAsync(_fi->low_bound, _wq->data, _wq->datasize, __on_donewr, this);

        // then update the bounds in the file_info and storage
        _wq->_q = update_bounds;
      }

      break; case update_bounds:{
        DEBUG_PRINT("\tstate: update_bounds\n");
        // just write the bounds to storage
        write_queue *_wq = reinterpret_cast<write_queue*>(queue_edit[0]);

        _fi = _get_fileinfo(_wq->id);
        _tmpdata_new(_fileinfo_size());
        DEBUG_PRINT("lb %d, hb %d\n", _fi->low_bound, _fi->high_bound);

        ByteIteratorR _bir{_tmpdata, _tmpdatasize};
        _bir.setVar(reinterpret_cast<char*>(&_fi->id), _sizebytes);
        _bir.setVar(reinterpret_cast<char*>(&_fi->low_bound), _sizebytes);
        _bir.setVar(reinterpret_cast<char*>(&_fi->high_bound), _sizebytes);

        bound_storage->bufWriteAsync(_fi->file_info_address, _tmpdata, _tmpdatasize, __on_donewr, this);
        _wq->_q = done;
      }


      break; case defrag:{
        DEBUG_PRINT("\tstate: defrag\n");
        // sort by bounds, then repack it from the highest bound
        if(!_defrag_donesorting){
          _defrag_donesorting = true;
          _defrag_reading = true;

          // clear again, just in case
          _sfinfo.clear();
          _sfinfo.reserve(f_info.size());
          for(auto &fi: f_info)
            _sfinfo.push_back(fi);
        
          std::sort(_sfinfo.begin(), _sfinfo.end(), file_info::sort_bound);

          // set the next address on the end of the storage
          _defrag_nextaddress = bound_storage->getMemSize();
        }

        if(_defrag_reading){
          _fi = &_sfinfo[_sfinfo.size()-1];
          _tmpdata_new(_fi->high_bound-_fi->low_bound);

          bound_storage->bufReadAsync(_fi->low_bound, _tmpdata, _tmpdatasize, __on_donewr, this);
          _defrag_reading = false;
        }
        else{
          _fi = &_sfinfo[_sfinfo.size()-1];
          _fi->high_bound = _defrag_nextaddress;
          _defrag_nextaddress -= _tmpdatasize;
          _fi->low_bound = _defrag_nextaddress;

          _lowest_address = _fi->low_bound;

          // copy the file to new location
          bound_storage->bufWriteAsync(_defrag_nextaddress, _tmpdata, _tmpdatasize, __on_donewr, this);

          // updating the info to f_info
          *std::find(f_info.begin(), f_info.end(), *_fi) = *_fi;

          _defrag_reading = true;
          _sfinfo.pop_back();

          // reset all variables
          if(!_sfinfo.size()){
            // updating all id table
            queue_edit[0][0] = update_allidtable;
            _defrag_donesorting = false;
          }
        }
      }

      break; case update_allidtable:{
        DEBUG_PRINT("\tstate: update_allidtable\n");
        size_t _addr = _header_size();
        _tmpdata_new(_sizebytes + _fileinfo_size()*f_info.size());

        ByteIteratorR _bir{_tmpdata, _tmpdatasize};
        uint32_t _fsize = f_info.size();
        _bir.setVar(reinterpret_cast<char*>(&_fsize), _sizebytes);
        
        for(int i = 0; i < f_info.size(); i++){
          f_info[i].file_info_address = _addr;
          _addr += _fileinfo_size();

          _bir.setVar(reinterpret_cast<char*>(&f_info[i].id), _sizebytes);
          _bir.setVar(reinterpret_cast<char*>(&f_info[i].low_bound), _sizebytes);
          _bir.setVar(reinterpret_cast<char*>(&f_info[i].high_bound), _sizebytes);
        }

        bound_storage->bufWriteAsync(_header_size(), _tmpdata, _tmpdatasize, __on_donewr, this);
        queue_edit[0][0] = done;
      }
    }
  }
}

void file_system::_finishwr(){
  // deleting file info that has future size of 0
  for(int i = 0; i < f_info.size(); i++){
    auto _fi = &f_info[i];
    if(!_fi->_future_size){
      // getting from the storage (so it doesn't need to sort, in order to find the highest bound)
      // then the file_info with the highest bound will be allocated to the deleted file_info
      size_t _memaddr = _header_fullsize() - _fileinfo_size();
      uint32_t _id;
      bound_storage->bufReadBlock(_memaddr, reinterpret_cast<char*>(&_id), _sizebytes);

      // if deleted id isn't the same than the last id, move the last id to the deleted
      // else, don't do anything
      if(_id != _fi->id){
        file_info __fi{.id = _id};
        auto _fiiter = std::find(f_info.begin(), f_info.end(), __fi);

        size_t _fisize = _fileinfo_size();
        char *_data = (char*)malloc(_fisize);
        ByteIteratorR _w{_data, _fisize};
        _w.setVar(reinterpret_cast<char*>(&_fiiter->id), _sizebytes);
        _w.setVar(reinterpret_cast<char*>(&_fiiter->low_bound), _sizebytes);
        _w.setVar(reinterpret_cast<char*>(&_fiiter->high_bound), _sizebytes);
        _fiiter->file_info_address = _fi->file_info_address;

        bound_storage->bufWriteBlock(_fiiter->file_info_address, _data, _fisize);
      }

      f_info.erase(f_info.begin() + i);
    }
  }

  size_t _nsize = f_info.size();
  bound_storage->bufWriteBlock(_header_size(), reinterpret_cast<char*>(&_nsize), _sizebytes);

  _notready = false;
}

size_t file_system::_header_size(){
  return (sizeof(uint16_t) + sizeof(uint8_t));
}

size_t file_system::_header_fullsize(){
  return __header_fullsize(f_info.size());
}

size_t file_system::__header_fullsize(size_t _s){
  return
    _header_size() +                              // header
    _sizebytes +                                  // file_info array size
    (_fileinfo_size() * _s)            // file_info array
  ;
}

size_t file_system::_fileinfo_size(){
  return _sizebytes*3;
}

size_t file_system::_bottom_size(){
  // 0x03 is the offset from the header
  return 
    _lowest_address -
    _header_fullsize()
  ;
}

file_system::file_info *file_system::_get_fileinfo(uint16_t file_id){
  file_info __ficomp{.id = file_id};
  auto _fiiter = std::find(f_info.begin(), f_info.end(), __ficomp);

  if(_fiiter == f_info.end())
    return NULL;
  else
    return &*_fiiter;
}

file_system::file_info *file_system::_file_exist(uint16_t file_id){
  // this will be using future state (means using _future_size as the parameter)
  auto _fi = _get_fileinfo(file_id);
  if(_fi && _fi->_future_size)
    return _fi;
  
  return NULL;
}


fs_error file_system::init_storage(EEPROM_Class *storage){
  uint16_t header = FILESYSTEM_HEADERCODE;
  storage->bufWriteBlock(0x00, reinterpret_cast<char*>(&header), sizeof(uint16_t));

  // determining bytes used for size based on storage size
  uint8_t __sizebytes = 0;
  uint32_t _n = 0;
  for(int i = 0; i < sizeof(uint32_t); i++){
    _n = _n << 8;
    _n |= 0xFF;

    if(storage->getMemSize() < _n){
      __sizebytes = i+1;
      break;
    }
  }

  storage->bufWriteBlock(0x02, reinterpret_cast<char*>(&__sizebytes), sizeof(uint8_t));
  
  // how many the file info are
  uint32_t dmp = 0;
  storage->bufWriteBlock(0x03, reinterpret_cast<char*>(&dmp), __sizebytes);

  return fs_error::ok;
}

fs_error file_system::
bind_storage(EEPROM_Class *storage){
  if(_notready)
    return fs_error::storage_busy;
  
  // initializing class variables
  bound_storage = NULL;
  f_info.clear();
  _future_storagesize = storage->getMemSize();
  DEBUG_PRINT("_currentsize %d\n", _future_storagesize);

  // storage initializing
  uint16_t header = 0;
  storage->bufReadBlock(0x00, reinterpret_cast<char*>(&header), sizeof(uint16_t));
  if(header != FILESYSTEM_HEADERCODE)
    return fs_error::storage_not_initiated;
  
  storage->bufReadBlock_currAddr(reinterpret_cast<char*>(&_sizebytes), sizeof(uint8_t));
  DEBUG_PRINT("sb %d\n", _sizebytes);
  if(_sizebytes == 0 || _sizebytes > sizeof(uint32_t))
    return fs_error::fs_fault;

  uint32_t fileslen = 0;
  storage->bufReadBlock_currAddr(reinterpret_cast<char*>(&fileslen), _sizebytes);

  int corrupt_f = 0;
  f_info.resize(fileslen);
  _lowest_address = storage->getMemSize();
  for(int i = 0; i < fileslen; i++){
    file_info f{.high_bound = 0, .low_bound = 0};
    f.file_info_address = storage->getMemAddress();
    storage->bufReadBlock_currAddr(reinterpret_cast<char*>(&f.id), sizeof(uint16_t));
    storage->bufReadBlock_currAddr(reinterpret_cast<char*>(&f.low_bound), _sizebytes);
    storage->bufReadBlock_currAddr(reinterpret_cast<char*>(&f.high_bound), _sizebytes);

    if(f.low_bound > f.high_bound){
      corrupt_f++;
      continue;
    }

    if(f.low_bound < _lowest_address)
      _lowest_address = f.low_bound;
    
    f._future_size = f.high_bound-f.low_bound;
    _future_storagesize -= f._future_size;
    DEBUG_PRINT("fhb %d, flb %d\n", f.high_bound, f.low_bound);
    DEBUG_PRINT("size %d, csize %d\n", f._future_size, _future_storagesize);

    f_info[i-corrupt_f] = f;
  }
  f_info.resize(fileslen-corrupt_f);
  DEBUG_PRINT("_currentsize %d\n", _future_storagesize);

  std::sort(f_info.begin(), f_info.end());
  bound_storage = storage;
  _future_storagesize -= _header_fullsize();
  DEBUG_PRINT("_currentsize %d\n", _future_storagesize);
  _notready = false;
  return fs_error::ok;
}


fs_error file_system::read_file(uint16_t file_id, char *data, fs_readcb cb, void *obj){
  if(!bound_storage)
    return fs_error::storage_not_bound;
  
  if(!file_exist(file_id))
    return fs_error::file_not_found;
  
  read_queue *q = (read_queue*)malloc(sizeof(read_queue));
  *q = read_queue{
    .id = file_id,
    .data = data,
    .cb = cb,
    .obj = obj
  };

  queue_edit.push_back(reinterpret_cast<char*>(q));
  _check_queue();
  return fs_error::ok;
}


fs_error file_system::write_file(uint16_t file_id, char *data, size_t datasize){
  if(!bound_storage)
    return fs_error::storage_not_bound;
  
  DEBUG_PRINT("fid %d\n", file_id);
  auto _fi = _get_fileinfo(file_id);
  DEBUG_PRINT("futuresize %d, datasize %d\n", _future_storagesize, datasize);
  if(_fi){
    if(_future_storagesize < (datasize - _fi->_future_size))
      return fs_error::file_too_big;
  }
  else{
    if((_future_storagesize - _fileinfo_size()) < datasize)
      return fs_error::file_too_big;
  }

  write_queue *q = (write_queue*)malloc(sizeof(write_queue));
  *q = write_queue{
    .id = file_id,
    .data = data,
    .datasize = datasize
  };

  // if file_id not found, then add the file_id to storage
  if(!_fi){
    file_info __ficomp{
      .id = file_id,
      .file_info_address = _header_fullsize(),
      .high_bound = 0,
      .low_bound = 0,
      ._future_size = datasize
    };
    _future_storagesize -= datasize + _fileinfo_size();

    auto _fi_iter = std::upper_bound(f_info.begin(), f_info.end(), __ficomp);
    f_info.insert(_fi_iter, __ficomp);
  }
  else{
    _future_storagesize += _fi->_future_size-datasize;
    _fi->_future_size = datasize;
  }

  DEBUG_PRINT("q: %d\n", q->_q);

  queue_edit.push_back(reinterpret_cast<char*>(q));
  _check_queue();
  return fs_error::ok;
}

fs_error file_system::delete_file(uint16_t file_id){
  if(!bound_storage)
    return fs_error::storage_not_bound;
  
  auto _fi = _file_exist(file_id);
  if(!_fi)
    return fs_error::file_not_found;
  
  _future_storagesize += _fi->_future_size;
  _fi->_future_size = 0;
  
  // if queue is empty, create dummy queue, then run
  if(!queue_edit.size()){
    char *_dmp = (char*)malloc(1);
    *_dmp = _queue_enum::done;
    queue_edit.push_back(_dmp);

    _on_donewr();
  }

  return fs_error::ok;
}


size_t file_system::file_size(uint16_t file_id){
  auto _fi = _file_exist(file_id);
  if(!_fi)
    return 0;
  
  return _fi->_future_size;
}

bool file_system::file_exist(uint16_t file_id){
  // this will be using future state (means using _future_size as the parameter)
  return _file_exist(file_id) != NULL;
}

size_t file_system::storage_free(){
  return _future_storagesize;
}

bool file_system::is_busy(){
  return _notready;
}

float file_system::frag_percentage(){
  DEBUG_PRINT("sf %d, bs %d\n", storage_free(), _bottom_size());
  return (float)(storage_free()-_bottom_size())/bound_storage->getMemSize();
}

fs_error file_system::storage_defrag(){
  if(!bound_storage)
    return fs_error::storage_not_bound;

  defrag_queue *dq = (defrag_queue*)malloc(sizeof(defrag_queue));
  *dq = defrag_queue{};
  queue_edit.push_back(reinterpret_cast<char*>(dq));
  _check_queue();

  return fs_error::ok;
}

void file_system::complete_tasks(){
  YieldWhile(queue_edit.size() && is_busy());
}


file_system FS;