#ifndef CHARTNAVIGATION_TYPEERASURE_HPP
#define CHARTNAVIGATION_TYPEERASURE_HPP


#include <concepts>
#include <memory>


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

#endif //CHARTNAVIGATION_TYPEERASURE_HPP
