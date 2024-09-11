#ifndef SINGLETON_HPP
#define SINGLETON_HPP

#include <memory>
#include <mutex>

namespace nd {
//---------------------------------------------------------------------------
// DataType normal
template <typename DataType, int instanceId = 0>
class Singleton {
 public:
  static DataType* Instance() {
    if (NULL == m_data_holder.get()) {
      std::lock_guard<std::mutex> lock(m_db_lock_mutex);
      if (NULL == m_data_holder.get()) {
        m_data_holder.reset(new DataType);
      }
    }
    return m_data_holder.get();
  }

 private:
  Singleton() {};
  ~Singleton() {};

  static std::shared_ptr<DataType> m_data_holder;
  static std::mutex m_db_lock_mutex;
};

template <typename DataType, int instanceId>
std::shared_ptr<DataType> Singleton<DataType, instanceId>::m_data_holder;

template <typename DataType, int instanceId>
std::mutex Singleton<DataType, instanceId>::m_db_lock_mutex;

//---------------------------------------------------------------------------
// DataType with param
template <typename DataType, int instanceId = 0>
class ParamSingleton {
 public:
  static DataType* Instance() {
    if (NULL == m_data_holder.get()) {
      std::lock_guard<std::mutex> lock(m_db_lock_mutex);
      if (NULL == m_data_holder.get()) {
        m_data_holder.reset(new DataType(instanceId));
      }
    }
    return m_data_holder.get();
  }

 private:
  ParamSingleton() {};
  ~ParamSingleton() {};

  static std::shared_ptr<DataType> m_data_holder;
  static std::mutex m_db_lock_mutex;
};

template <typename DataType, int instanceId>
std::shared_ptr<DataType> ParamSingleton<DataType, instanceId>::m_data_holder;

template <typename DataType, int instanceId>
std::mutex ParamSingleton<DataType, instanceId>::m_db_lock_mutex;

//---------------------------------------------------------------------------
// DataType with init
template <typename DataType, int instanceId = 0>
class InitDataSingleton {
 public:
  static DataType* Instance() {
    if (NULL == m_data_holder.get()) {
      std::lock_guard<std::mutex> lock(m_db_lock_mutex);
      if (NULL == m_data_holder.get()) {
        DataType* data = new DataType;
        data->init();
        m_data_holder.reset(data);
      }
    }
    return m_data_holder.get();
  }

 private:
  InitDataSingleton() {};
  ~InitDataSingleton() {};

  static std::shared_ptr<DataType> m_data_holder;
  static std::mutex m_db_lock_mutex;
};

template <typename DataType, int instanceId>
std::shared_ptr<DataType> InitDataSingleton<DataType, instanceId>::m_data_holder;

template <typename DataType, int instanceId>
std::mutex InitDataSingleton<DataType, instanceId>::m_db_lock_mutex;

//---------------------------------------------------------------------------
// DataType with init param
template <typename DataType, int instanceId = 0>
class InitParamSingleton {
 public:
  static DataType* Instance() {
    if (NULL == m_data_holder.get()) {
      std::lock_guard<std::mutex> lock(m_db_lock_mutex);
      if (NULL == m_data_holder.get()) {
        DataType* data = new DataType;
        data->init(instanceId);
        m_data_holder.reset(data);
      }
    }
    return m_data_holder.get();
  }

 private:
  InitParamSingleton() {};
  ~InitParamSingleton() {};

  static std::shared_ptr<DataType> m_data_holder;
  static std::mutex m_db_lock_mutex;
};

template <typename DataType, int instanceId>
std::shared_ptr<DataType> InitParamSingleton<DataType, instanceId>::m_data_holder;

template <typename DataType, int instanceId>
std::mutex InitParamSingleton<DataType, instanceId>::m_db_lock_mutex;
}  // namespace nd

#endif /* SINGLETON_HPP */
