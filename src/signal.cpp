#include <native.h>
#include <map>

namespace native
{

    template <typename... Args>
    signal<Args...>::signal() : current_id(0), initialized(false), initializer(nullptr) {}

    template <typename... Args>
    signal<Args...>::signal(std::function<void()> init)
        : signal()
    {
        initializer = std::move(init);
    }

    template <typename... Args>
    int signal<Args...>::connect(const std::function<bool(Args...)> &slot) const
    {
        ensure_init();
        slots[++current_id] = slot;
        return current_id;
    }

    template <typename... Args>
    void signal<Args...>::disconnect(int id) const
    {
        slots.erase(id);
    }

    template <typename... Args>
    void signal<Args...>::disconnect_all() const
    {
        slots.clear();
    }

    template <typename... Args>
    void signal<Args...>::emit(Args... args)
    {
        ensure_init();
        for (auto it = slots.rbegin(); it != slots.rend(); ++it)
        {
            if (it->second(std::forward<Args>(args)...))
                break;
        }
    }

    template <typename... Args>
    void signal<Args...>::ensure_init() const
    {
        if (!initialized && initializer)
        {
            initializer();
            initialized = true;
        }
    }

    // Force template instantiations (if needed for fixed arities)
    template class signal<>;
    template class signal<int>;
    template class signal<int, int>;
    template class signal<const std::string &>;

} // namespace native
