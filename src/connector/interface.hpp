#ifndef CHARTNAVIGATION_INTERFACE_HPP
#define CHARTNAVIGATION_INTERFACE_HPP

#include <vector>
#include <span>


struct DatarefIdx {
    size_t idx;
};

class InterfaceSimu {
    public:
        virtual ~InterfaceSimu () = default;

        virtual void setCallback (const std::function<void  (bool)> &callbackFunc) =0;
        virtual void close () =0;

        virtual DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq) =0;
        virtual bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) =0;
};


// 好像用内存擦除的模式也挺不错的
template <typename T>
concept Drawable = requires(const T t)
{
    { t.draw() } -> std::same_as<void>;
};
class Shape {
    public:
        template <Drawable T>
        explicit Shape (T &&x)
            : self(std::make_unique<Model<std::decay_t<T>>>(std::forward<T>(x))) {}
        Shape (Shape &&) noexcept = default;
        Shape& operator= (Shape &&) noexcept = default;
        void draw () const {
            if (self) self->draw_impl();
        }
    private:
        struct Interface {
            virtual ~Interface () = default;
            virtual void draw_impl () const = 0;
        };
        template <typename T>
        struct Model final : Interface {
            explicit Model (T x) : data(std::move(x)) {}
            void draw_impl () const override {
                data.draw();
            }
            T data;
        };

        std::unique_ptr<Interface> self;
};

#endif //CHARTNAVIGATION_INTERFACE_HPP
