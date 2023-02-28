#ifndef FILE_SYSTEM
#define FILE_SYSTEM

#include "EEPROM.hpp"
#include "vector"
#include "queue"


enum fs_error{
  file_too_big,
  file_not_found,
  storage_not_bound,
  storage_not_initiated,
  storage_busy,
  fs_fault,
  ok
};


// almost all function will run asynchronously
class file_system{
  public:
    typedef void (*fs_readcb)(void*);

  private:
    struct file_info{
      friend bool operator<(const file_info &f, const file_info &f1){
        return f.id < f1.id;
      }

      friend bool operator==(const file_info &f, const file_info &f1){
        return f.id == f1.id;
      }

      static bool sort_bound(const file_info &f1, const file_info &f2){
        return f1.low_bound < f2.high_bound;
      }

      uint16_t id;
      uint32_t file_info_address;
      uint32_t high_bound, low_bound;


      // Future State variables

      // this will changed, asap, but not reflect the actual size
      // if this variable set to 0, it means that the file will be deleted
      //
      // if _future_size keeps on 0 until deleting queue then it will be erased from f_info
      uint32_t _future_size;
    };


    enum _queue_enum{
      done,
      done_r,
      done_w,
      read,
      defrag,
      write,
      update_bounds,
      update_allidtable,
      update_filecount
    };


    // NOTE: don't use id address, as it can changed when defragging or deleted afterward
    struct read_queue{
      uint8_t _q = file_system::_queue_enum::read;
      uint32_t id;
      char *data;
      int datasize;
      fs_readcb cb;
      void *obj;
    };

    struct defrag_queue{
      uint8_t _q = file_system::_queue_enum::defrag;
    };

    // or adding
    struct write_queue{
      uint8_t _q = file_system::_queue_enum::write;
      uint32_t id;
      char *data;
      size_t datasize;
    };

    // current state variables
    uint32_t _lowest_address;

    // future state variables
    uint32_t _future_storagesize;

    // this will be instantly added when write_file called (in adding context)
    // and instantly deleted when delete queue and the _future_size is 0
    std::vector<file_info> f_info;
    std::vector<char*> queue_edit;
    EEPROM_Class *bound_storage = NULL;

    uint8_t _sizebytes;
    bool _notready = false;
    bool _update_filecount = false;


    // variables for read/write task
    char *_tmpdata = NULL;
    size_t _tmpdatasize;
    size_t _defrag_nextaddress;
    bool _defrag_reading = true;
    bool _defrag_donesorting = false;
    std::vector<file_info> _sfinfo;

    void _tmpdata_new(size_t memsize);
    void _tmpdata_free();

    void _check_queue();
    void _on_donewr();
    static void __on_donewr(void *obj){
      ((file_system*)obj)->_on_donewr();
    }

    void _finishwr();


    size_t _header_size();
    size_t _header_fullsize();
    size_t __header_fullsize(size_t _size);
    size_t _fileinfo_size();

    // EEPROM size on bottom side
    size_t _bottom_size();

    // if file_info is NULL, then the file isn't exist
    // this is based on current state
    file_info *_get_fileinfo(uint16_t file_id);

    // if file_info is NULL, then the file isn't exist
    // this is based on future state
    file_info *_file_exist(uint16_t file_id);

  
  public:
    // NOTE: blocking function
    static fs_error init_storage(EEPROM_Class *storage);

    // NOTE: blocking function
    fs_error bind_storage(EEPROM_Class *storage);

    // made sure that the buffer is enough to contain file data
    fs_error read_file(uint16_t file_id, char *data, fs_readcb callback, void *cobj, int _readsize = -1);

    // don't free memory outside the function, let the func handle it
    fs_error write_file(uint16_t file_id, char *data, size_t datasize);
    fs_error delete_file(uint16_t file_id);

    // will return 0, if the file doens't exist
    // make sure this object isn't busy, as the update will only applied whenever the file is finished update
    size_t file_size(uint16_t file_id);
    bool file_exist(uint16_t file_id);

    // actual EEPROM size
    size_t storage_free();

    // checking if still busy
    bool is_busy();
    bool is_storage_bound();

    // returns how big the fragmentation hole taking up the space
    // returns in range 0 to 1
    float frag_percentage();

    // defragging the storage
    // will not return ok if the frag percentage is 0
    fs_error storage_defrag();

    void complete_tasks();
};

extern file_system EEPROM_FS;

#endif