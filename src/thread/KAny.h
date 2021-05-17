

#ifndef _ANY_HPP_
#define _ANY_HPP_

#include <typeinfo>
#include <iostream>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace klib {
    /*
    该类可以用来存储任意数据类型
    如果数据类型为指针，则只存储指针，不会存储指针所指向的内容，即不会深度拷贝指针指向的内容
    */
    class KAny
    {
        // 抽象类
        class ValueHolderBase
        {
        public:
            // 析构函数 //
            virtual ~ValueHolderBase() {}
            // 复制函数 //
            virtual ValueHolderBase* Clone() const = 0;
            virtual void Copy(ValueHolderBase* r) = 0;
            // 获取数据类型函数，默认类型void //
            virtual const std::type_info& TypeInfo() const = 0;
        };

        // 数据存储类 //
        template<typename ValueType>
        class ValueHolder : public ValueHolderBase
        {
        public: 
            // 构造函数 //
            ValueHolder(const ValueType& val) : m_value(val){}
            // 复制数据 //
            virtual ValueHolderBase* Clone() const { return new ValueHolder<ValueType>(m_value); }
            // 拷贝同类型数据 //
            virtual void Copy(ValueHolderBase* r)
            {
                ValueHolder<ValueType>* s = dynamic_cast<ValueHolder<ValueType>*>(r);
                m_value = s->GetValue();
            }
            // 获取数据类型 //
            virtual const std::type_info& TypeInfo() const{ return typeid(ValueType); }
            // 获取数据 //
            const ValueType& GetValue() { return m_value; }
            void SetValue(const ValueType& v) { m_value = v; }
        private:
            ValueType m_value;
        };
    public:
        // 默认构造 //
        KAny() :m_dat(NULL) {}
        // 构造函数 //
        template<typename ValueType>
        KAny(const ValueType& v) :m_dat(new ValueHolder<ValueType>(v)) {}
        // 拷贝构造 //
        KAny(const KAny& rhs) 
		:m_dat(NULL)
        {
            if (rhs.m_dat)
                m_dat = rhs.m_dat->Clone();
        }
        // 析构函数 //
        virtual ~KAny(){ Release(); }
        // 赋值操作符重载 //
        template<typename ValueType>
        KAny& operator=(const ValueType& v)
        {
            if (TypeInfo() == typeid(ValueType))
            {
                ValueHolder<ValueType>* s = dynamic_cast<ValueHolder<ValueType>*>(m_dat);
                s->SetValue(v);
            }
            else
            {
                Release();
                m_dat = new ValueHolder<ValueType>(v);
            }
            return *this;
        }
        // 赋值操作符重载 //
        KAny& operator=(const KAny& rhs)
        {
            if (this == &rhs)
                return *this;
            if (TypeInfo() == rhs.TypeInfo())
                m_dat->Copy(rhs.m_dat);
            else
            {
                Release();
                if (rhs.m_dat)
                    m_dat = rhs.m_dat->Clone();
            }
            return *this;
        }
        // 获取数据类型 //
        virtual const std::type_info& TypeInfo() const
        {
            if (!m_dat)
                return typeid(void);
            return m_dat->TypeInfo();
        }
        // 将数据转换为指定类型 //
        template<typename ValueType>
        static const ValueType& AnyCast(const KAny& a)
        {
            if (!a.m_dat)
                throw std::invalid_argument("KAny data is null");

            ValueHolder<ValueType>* s = dynamic_cast<ValueHolder<ValueType>*>(a.m_dat);
            if (s)
                return s->GetValue();
            else
                throw std::bad_cast();
        }

    private:
        // 释放资源 //
        void Release()
        {
            if (m_dat)
            {
                delete m_dat;
                m_dat = NULL;
            }
        }

    private:
        ValueHolderBase* m_dat;
    };
};
#endif //_ANY_HPP_
