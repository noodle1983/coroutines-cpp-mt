#ifndef SINGLETON_HPP
#define SINGLETON_HPP

#include <mutex>
#include <memory>

namespace nd
{
    //---------------------------------------------------------------------------
    //DataType normal
    template<typename DataType, int instanceId = 0>
    class Singleton 
    {
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
        Singleton(){};
        ~Singleton(){};

        static std::shared_ptr<DataType> m_data_holder;
        static std::mutex m_db_lock_mutex;
    };

    template <typename DataType, int instanceId>
    std::shared_ptr<DataType> Singleton<DataType, instanceId>::m_data_holder;

    template <typename DataType, int instanceId>
    std::mutex Singleton<DataType, instanceId>::m_db_lock_mutex;

    //---------------------------------------------------------------------------
    //DataType with param
    template<typename DataType, int instanceId = 0>
    class ParamSingleton 
    {
    public:
     static DataType* Instance() {
       if (NULL == data_holder_m.get()) {
         std::lock_guard<std::mutex> lock(db_lock_mutex_m);
         if (NULL == data_holder_m.get()) {
           data_holder_m.reset(new DataType(instanceId));
         }
       }
       return data_holder_m.get();
     }

    private:
        ParamSingleton(){};
        ~ParamSingleton(){};

        static std::shared_ptr<DataType> data_holder_m;
        static std::mutex db_lock_mutex_m;
    };

    template <typename DataType, int instanceId>
    std::shared_ptr<DataType>
        ParamSingleton<DataType, instanceId>::data_holder_m;

    template <typename DataType, int instanceId>
    std::mutex ParamSingleton<DataType, instanceId>::db_lock_mutex_m;

    //---------------------------------------------------------------------------
	//DataType with init
    template<typename DataType, int instanceId = 0>
    class InitDataSingleton 
    {
    public:
     static DataType* Instance() {
       if (NULL == data_holder_m.get()) {
         std::lock_guard<std::mutex> lock(db_lock_mutex_m);
         if (NULL == data_holder_m.get()) {
           DataType* data = new DataType;
           data->init();
           data_holder_m.reset(data);
         }
       }
       return data_holder_m.get();
     }

    private:
        InitDataSingleton(){};
        ~InitDataSingleton(){};

        static std::shared_ptr<DataType> data_holder_m;
        static std::mutex db_lock_mutex_m;
    };

    template <typename DataType, int instanceId>
    std::shared_ptr<DataType>
        InitDataSingleton<DataType, instanceId>::data_holder_m;

    template <typename DataType, int instanceId>
    std::mutex InitDataSingleton<DataType, instanceId>::db_lock_mutex_m;

    //---------------------------------------------------------------------------
	//DataType with init param
    template<typename DataType, int instanceId = 0>
    class InitParamSingleton 
    {
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
        InitParamSingleton(){};
        ~InitParamSingleton(){};

        static std::shared_ptr<DataType> m_data_holder;
        static std::mutex m_db_lock_mutex;
    };

    template <typename DataType, int instanceId>
    std::shared_ptr<DataType>
        InitParamSingleton<DataType, instanceId>::m_data_holder;

    template <typename DataType, int instanceId>
    std::mutex InitParamSingleton<DataType, instanceId>::m_db_lock_mutex;
}

#endif /* SINGLETON_HPP */

