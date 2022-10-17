#include "file_system.hpp"
#include "ByteIterator.hpp"

#include "algorithm"

#define FILESYSTEM_HEADERCODE 0x1632

// TODO do code review before going on debugging hell
void file_system::_check_queue(){
  if(!_notready && queue_edit.size()){
    _notready = true;
    _on_donewr();
  }
}

void file_system::_on_donewr(){
  if(queue_edit.size()){
    uint8_t _qtype = queue_edit[0][0];
    file_info *_fi;
    switch((_queue_enum)_qtype){

      // after this, it will continue to case 'done'
      break; case done_r:{
        // calling to specified callback
        read_queue *_rq = reinterpret_cast<read_queue*>(queue_edit[0]);
        _rq->cb(_rq->obj);
      }
      
      case done:{
        // free the queue, end then erase it from the queue
        free(queue_edit[0]);
        queue_edit.erase(queue_edit.begin());

        // if the queue still not empty, calling this function
        if(queue_edit.size())
          _on_donewr();
        else
          _notready = false;
      }

      break; case read:{
        // just read to specific address
        read_queue *_rq = reinterpret_cast<read_queue*>(queue_edit[0]);
        _fi = _get_fileinfo(_rq->id);

        bound_storage->bufReadAsync(_fi->low_bound, _rq->data, _fi->high_bound-_fi->low_bound, __on_donewr, this);
        _rq->_q = done_r;
      }

      // after this, it will continue to case 'write'
      break; case add_fileid:{
        // determine the new file_info_address
        write_queue *_wq = reinterpret_cast<write_queue*>(queue_edit[0]);
        _fi = _get_fileinfo(_wq->id);
        _fi->file_info_address = _header_fullsize();

        // updating the bounds will be handled in case 'write'
        _wq->_q = write;
      }

      case write:{
        write_queue *_wq = reinterpret_cast<write_queue*>(queue_edit[0]);
        if(_qtype != add_fileid)
          _fi = _get_fileinfo(_wq->id);

        // if datasize is higher than high_bound-low_bound, then reallocate the address
        // if not, just reuse the address
        if(_wq->datasize > (_fi->high_bound-_fi->low_bound)){
          if(_bottom_size() < _wq->datasize){
            defrag_queue *dq = (defrag_queue*)malloc(sizeof(defrag_queue));
            *dq = defrag_queue{};

            queue_edit.insert(queue_edit.begin(), reinterpret_cast<char*>(dq));
            return;
          }

          _fi->high_bound = _lowest_address;
          _lowest_address -= _wq->datasize;
        }

        _fi->low_bound = _fi->high_bound-_wq->datasize;

        // writing to storage
        bound_storage->bufWriteAsync(_fi->low_bound, _wq->data, _wq->datasize, __on_donewr, this);

        // then update the bounds in the file_info and storage
        _wq->_q = update_bounds;
      }

      break; case update_bounds:{
        // just write the bounds to storage
        write_queue *_wq = reinterpret_cast<write_queue*>(queue_edit[0]);

        _fi = _get_fileinfo(_wq->id);
        _tmpdata = (char*)malloc(_sizebytes*2);
        memcpy(_tmpdata, &_fi->low_bound, _sizebytes);
        memcpy(_tmpdata+_sizebytes, &_fi->high_bound, _sizebytes);

        bound_storage->bufWriteAsync(_fi->file_info_address+_sizebytes, _tmpdata, _sizebytes*2, __on_donewr, this);
        _wq->_q = done;
      }

      break; case delete_fileid:{
        delete_queue *_dq = reinterpret_cast<delete_queue*>(queue_edit[0]);
        file_info __fi{.id = _dq->id};
        auto _fiiter = std::find(f_info.begin(), f_info.end(), __fi);

        // getting from the storage (so it doesn't need to sort, in order to find the highest bound)
        // then the file_info with the highest bound will be allocated to the deleted file_info
        size_t _memaddr = _header_fullsize();
        uint32_t _id;
        bound_storage->bufReadBlock(_memaddr, reinterpret_cast<char*>(&_id), _sizebytes);

        if(_id != _dq->id){
          __fi.id = _id;
          auto _fiiter1 = std::find(f_info.begin(), f_info.end(), __fi);
          if(!_fi->_future_size){
            size_t _fisize = _fileinfo_size();
            char *_data = (char*)malloc(_fisize);
            ByteIteratorR _w{_data, _fisize};
            _w.setVar(reinterpret_cast<char*>(_fiiter1->id), _sizebytes);
            _w.setVar(reinterpret_cast<char*>(_fiiter1->low_bound), _sizebytes);
            _w.setVar(reinterpret_cast<char*>(_fiiter1->high_bound), _sizebytes);
            bound_storage->bufWriteBlock(_fiiter1->file_info_address, _data, _fisize);

            _fiiter1->file_info_address = _fiiter->file_info_address;
          }
        }

        _dq->_q = done;

        // if the _future_size is 0, then delete it from f_info
        if(!_fi->_future_size){
          f_info.erase(_fiiter);
          size_t _nsize = f_info.size();
          bound_storage->bufWriteAsync(_header_size(), reinterpret_cast<char*>(&_nsize), _sizebytes, __on_donewr, this);
        }
        else
          _on_donewr();
      }

      break; case defrag:{
        // sort by bounds, then repack it from the highest bound
        if(!_defrag_donesorting){
          _defrag_donesorting = true;

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
          _tmpdatasize = _fi->high_bound-_fi->low_bound;
          _tmpdata = (char*)malloc(_tmpdatasize);

          bound_storage->bufReadAsync(_fi->low_bound, _tmpdata, _tmpdatasize, __on_donewr, this);
          _defrag_reading = false;
        }
        else{
          _fi = &_sfinfo[_sfinfo.size()-1];
          _fi->high_bound = _defrag_nextaddress;
          _defrag_nextaddress -= _tmpdatasize;
          _fi->low_bound = _defrag_nextaddress;

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
        size_t _addr = _header_size();
        _tmpdatasize = _fileinfo_size()*f_info.size();
        _tmpdata = (char*)malloc(_tmpdatasize);

        ByteIteratorR _bir{_tmpdata, _tmpdatasize};
        for(int i = 0; i < f_info.size(); i++){
          f_info[i].file_info_address = _addr;
          _addr += _fileinfo_size();

          _bir.setVar(reinterpret_cast<char*>(f_info[i].id), _sizebytes);
          _bir.setVar(reinterpret_cast<char*>(f_info[i].low_bound), _sizebytes);
          _bir.setVar(reinterpret_cast<char*>(f_info[i].high_bound), _sizebytes);
        }

        bound_storage->bufWriteAsync(_header_size(), _tmpdata, _tmpdatasize, __on_donewr, this);
        queue_edit[0][0] = done;
      }
    }
  }
}

size_t file_system::_header_size(){
  return (sizeof(uint16_t) + sizeof(uint8_t));
}

size_t file_system::_header_fullsize(size_t _s){
  return
    _header_size() +                              // header
    _sizebytes +                                  // file_info array size
    (_fileinfo_size() * (_s? _s: f_info.size()))  // file_info array
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
  return &*std::find(f_info.begin(), f_info.end(), __ficomp);
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
  uint8_t __sizebytes;
  for(int i = 0; i < sizeof(uint32_t)-1; i++)
    if(storage->getMemSize() > (0xFFFFFFFF >> (sizeof(uint32_t)-i-1)))
      __sizebytes = i+1;
    else
      break;

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
  _current_size = storage->getMemSize();

  // storage initializing
  uint16_t header = 0;
  storage->bufReadBlock(0x00, reinterpret_cast<char*>(&header), sizeof(uint16_t));
  if(header != FILESYSTEM_HEADERCODE)
    return fs_error::storage_not_initiated;
  
  storage->bufReadBlock_currAddr(reinterpret_cast<char*>(&_sizebytes), sizeof(uint8_t));
  if(fs_fault == 0 || fs_fault > sizeof(uint32_t))
    return fs_error::fs_fault;

  uint32_t fileslen = 0;
  storage->bufReadBlock_currAddr(reinterpret_cast<char*>(&fileslen), _sizebytes);

  int corrupt_f = 0;
  f_info.resize(fileslen);
  _lowest_address = UINT32_MAX;
  for(int i = 0; i < fileslen; i++){
    file_info f;
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
    _current_size -= f._future_size;

    f_info[i-corrupt_f] = f;
  }
  f_info.resize(fileslen-corrupt_f);

  std::sort(f_info.begin(), f_info.end());
  bound_storage = storage;
  _current_size -= _header_fullsize();
  _future_storagesize = _current_size;
  return fs_error::ok;
}


fs_error file_system::read_file(uint16_t file_id, char *data, fs_readcb cb, void *obj){
  if(!bound_storage)
    return fs_error::storage_not_bound;
  
  if(!file_exist(file_id))
    return fs_error::file_not_found;
  
  _notready = true;
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
  
  if(_future_storagesize < datasize)
    return fs_error::file_too_big;

  _notready = true;

  write_queue *q = (write_queue*)malloc(sizeof(write_queue));
  *q = write_queue{
    .id = file_id,
    .data = data,
    .datasize = datasize
  };

  // if file_id not found, then add the file_id to storage
  auto _fi = _get_fileinfo(file_id);
  if(!_fi){
    q->_q = _queue_enum::add_fileid;

    file_info __ficomp{
      .id = file_id,
      ._future_size = datasize
    };
    _future_storagesize -= datasize;

    auto _fi_iter = std::upper_bound(f_info.begin(), f_info.end(), __ficomp);
    f_info.insert(_fi_iter, __ficomp);
  }
  else{
    _future_storagesize += _fi->_future_size-datasize;
    _fi->_future_size = datasize;
  }

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
  
  _notready = true;
  _future_storagesize += _fi->_future_size;
  _fi->_future_size = 0;

  delete_queue *q = (delete_queue*)malloc(sizeof(delete_queue));
  *q = delete_queue{
    .id = file_id
  };

  queue_edit.push_back(reinterpret_cast<char*>(q));
  _check_queue();
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