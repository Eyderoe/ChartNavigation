#ifndef CHARTNAVIGATION_OBSERVE_HPP
#define CHARTNAVIGATION_OBSERVE_HPP


#include <memory>


// 一种脱离Qt框架实现QPointer的方法. 不对, 感觉和普通的shared_ptr差不多啊
template <typename T>
class SafePtr {
    class Proxy {
        public:
            explicit Proxy (std::shared_ptr<T> ptr) : sPtr(ptr) {}
            T* operator-> () const noexcept { return sPtr.get(); }
        private:
            std::shared_ptr<T> sPtr;
    };
    public:
        explicit SafePtr (std::shared_ptr<T> sPtr) : wPtr(sPtr) {}
        Proxy operator-> () const;
        [[nodiscard]] bool expired () const;
    private:
        std::weak_ptr<T> wPtr;
};

template <typename T>
SafePtr<T>::Proxy SafePtr<T>::operator-> () const {
    auto sPtr = wPtr.lock();
    if (!sPtr)
        throw std::runtime_error("");
    return Proxy(sPtr);
}

template <typename T>
bool SafePtr<T>::expired () const {
    return wPtr.expired();
}

#endif //CHARTNAVIGATION_OBSERVE_HPP
