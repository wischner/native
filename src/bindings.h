#pragma once

#include <unordered_map>

namespace native
{

    // A = handle type (e.g. Widget, HWND, Window)
    // B = associated object type (e.g. wnd*, control*, app_wnd*)
    template <typename A, typename B>
    class bindings
    {
    public:
        void register_pair(const A &a, const B &b)
        {
            a_to_b_[a] = b;
            b_to_a_[b] = a;
        }

        void unregister_by_a(const A &a)
        {
            auto it = a_to_b_.find(a);
            if (it != a_to_b_.end())
            {
                b_to_a_.erase(it->second);
                a_to_b_.erase(it);
            }
        }

        void unregister_by_b(const B &b)
        {
            auto it = b_to_a_.find(b);
            if (it != b_to_a_.end())
            {
                a_to_b_.erase(it->second);
                b_to_a_.erase(it);
            }
        }

        B from_a(const A &a) const
        {
            auto it = a_to_b_.find(a);
            return it != a_to_b_.end() ? it->second : B{};
        }

        A from_b(const B &b) const
        {
            auto it = b_to_a_.find(b);
            return it != b_to_a_.end() ? it->second : A{};
        }

        void clear()
        {
            a_to_b_.clear();
            b_to_a_.clear();
        }

    private:
        std::unordered_map<A, B> a_to_b_;
        std::unordered_map<B, A> b_to_a_;
    };

} // namespace native
