#include <cstring>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gem.h>

#include <native.h>

#include "globals.h"

namespace
{
    struct menu_state
    {
        std::vector<OBJECT> tree;
        std::deque<std::string> strings;
        std::unordered_map<int, int> object_to_item_id;
        bool installed = false;
    };

    std::unordered_map<native::app_wnd *, menu_state> g_menu_states;

    void init_object(OBJECT *object,
                     WORD next,
                     WORD head,
                     WORD tail,
                     UWORD type,
                     UWORD flags,
                     UWORD state,
                     LONG spec,
                     WORD x,
                     WORD y,
                     WORD w,
                     WORD h)
    {
        object->ob_next = next;
        object->ob_head = head;
        object->ob_tail = tail;
        object->ob_type = type;
        object->ob_flags = flags;
        object->ob_state = state;
        object->ob_spec = spec;
        object->ob_x = x;
        object->ob_y = y;
        object->ob_width = w;
        object->ob_height = h;
    }

    LONG store_string(menu_state &state, std::string text)
    {
        state.strings.push_back(std::move(text));
        return (LONG) (intptr_t) state.strings.back().c_str();
    }

    std::string menu_title_text(const std::string &title)
    {
        return " " + title + " ";
    }

    std::string menu_item_text(const std::string &label, size_t width)
    {
        std::string text = "  " + label + " ";
        if (text.size() < width) {
            text.append(width - text.size(), ' ');
        }
        return text;
    }

    menu_state &build_menu(native::main_menu &menu, native::app_wnd &owner)
    {
        menu_state &state = g_menu_states[&owner];
        const auto &tops = menu.tops();
        const int top_count = static_cast<int>(tops.size());
        const int root_index = 0;
        const int bar_box_index = 1;
        const int titles_index = 2;
        int next_index = 3;

        state.tree.clear();
        state.strings.clear();
        state.object_to_item_id.clear();
        state.installed = false;

        std::vector<int> title_indices;
        title_indices.reserve(tops.size());

        for (const auto &top : tops) {
            title_indices.push_back(next_index++);
        }
        const int popups_index = next_index++;

        std::vector<int> popup_indices;
        std::vector<int> first_item_indices;
        std::vector<int> last_item_indices;
        popup_indices.reserve(tops.size());
        first_item_indices.reserve(tops.size());
        last_item_indices.reserve(tops.size());
        for (const auto &top : tops) {
            const int popup_index = next_index++;
            popup_indices.push_back(popup_index);

            if (!top.items.empty()) {
                const int first_item_index = next_index;
                const int last_item_index = next_index + static_cast<int>(top.items.size()) - 1;
                first_item_indices.push_back(first_item_index);
                last_item_indices.push_back(last_item_index);
                next_index += static_cast<int>(top.items.size());
            } else {
                first_item_indices.push_back(NIL);
                last_item_indices.push_back(NIL);
            }
        }

        state.tree.resize(next_index);

        init_object(&state.tree[root_index], NIL, bar_box_index,
                    top_count > 0 ? title_indices.front() : bar_box_index,
                    G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0);
        init_object(&state.tree[bar_box_index], root_index, titles_index,
                    titles_index, G_BOX, NONE, NORMAL, 0x1100L, 0, 0, 80, 1);
        init_object(&state.tree[titles_index], bar_box_index,
                    top_count > 0 ? title_indices.front() : NIL,
                    top_count > 0 ? title_indices.back() : NIL,
                    G_IBOX, NONE, NORMAL, 0L, 0, 0, 80, 1);
        init_object(&state.tree[popups_index], root_index,
                    top_count > 0 ? popup_indices.front() : NIL,
                    top_count > 0 ? popup_indices.back() : NIL,
                    G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, 0);

        int title_x = 0;

        for (int top_i = 0; top_i < top_count; ++top_i) {
            const auto &top = tops[top_i];
            const int title_index = title_indices[top_i];
            const int popup_index = popup_indices[top_i];
            const int title_width = static_cast<int>(top.title.size()) + 2;
            const bool last_title = top_i == top_count - 1;
            const int item_count = static_cast<int>(top.items.size());
            const int first_item_index = first_item_indices[top_i];
            const int last_item_index = last_item_indices[top_i];
            size_t popup_width = 0;

            for (const auto &item : top.items) {
                popup_width = std::max(popup_width, item.label.size() + 3u);
            }
            if (popup_width < 8u) {
                popup_width = 8u;
            }

            init_object(&state.tree[title_index],
                        last_title ? titles_index : title_indices[top_i + 1],
                        NIL, NIL,
                        G_TITLE,
                        NONE,
                        NORMAL,
                        store_string(state, menu_title_text(top.title)),
                        static_cast<WORD>(title_x), 0,
                        static_cast<WORD>(title_width), 1);

            init_object(&state.tree[popup_index],
                        top_i == top_count - 1 ? popups_index : popup_indices[top_i + 1],
                        static_cast<WORD>(first_item_index),
                        static_cast<WORD>(last_item_index),
                        G_BOX, NONE, NORMAL, 0x1100L,
                        0, 0,
                        static_cast<WORD>(popup_width),
                        static_cast<WORD>(std::max(1, item_count)));

            for (int item_i = 0; item_i < item_count; ++item_i) {
                const int current_index = first_item_index + item_i;
                const bool last_item = item_i == item_count - 1;
                const auto &item = top.items[item_i];

                init_object(&state.tree[current_index],
                            last_item ? popup_index : current_index + 1,
                            NIL, NIL,
                            G_STRING,
                            NONE,
                            NORMAL,
                            store_string(state, menu_item_text(item.label, popup_width)),
                            0, static_cast<WORD>(item_i),
                            static_cast<WORD>(popup_width), 1);
                state.object_to_item_id[current_index] = item.id;
            }

            title_x += title_width;
        }

        if (next_index > 0) {
            state.tree[next_index - 1].ob_flags |= LASTOB;
        }

        return state;
    }
}

namespace native
{
    main_menu::~main_menu() = default;

    void main_menu::attach(app_wnd &owner)
    {
        _owner = &owner;
        if (_id == 0 && !_tops.empty()) {
            _id = 1;
        }
        if (_tops.empty()) {
            return;
        }

        menu_state &state = build_menu(*this, owner);
        menu_click(1, 1);
        menu_bar(state.tree.data(), 1);
        state.installed = true;
    }
}

namespace gemix
{
    OBJECT *menu_tree_for(native::app_wnd *owner)
    {
        auto it = g_menu_states.find(owner);
        if (it == g_menu_states.end() || it->second.tree.empty()) {
            return nullptr;
        }
        return it->second.tree.data();
    }

    int menu_item_id_for(native::app_wnd *owner, WORD object_index)
    {
        auto it = g_menu_states.find(owner);
        if (it == g_menu_states.end()) {
            return 0;
        }

        auto item_it = it->second.object_to_item_id.find(object_index);
        if (item_it == it->second.object_to_item_id.end()) {
            return 0;
        }
        return item_it->second;
    }

    void destroy_menu(native::app_wnd *owner)
    {
        auto it = g_menu_states.find(owner);
        if (it == g_menu_states.end()) {
            return;
        }
        if (it->second.installed) {
            menu_bar(it->second.tree.data(), 0);
        }
        g_menu_states.erase(it);
    }
}
