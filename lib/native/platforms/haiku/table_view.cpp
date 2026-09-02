//
// Implements materialized table_view with Haiku's ColumnListView and
// retains the compact BControlLook-backed host for virtual models.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include <Bitmap.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <Font.h>
#include <Message.h>
#include <String.h>
#include <View.h>
#include <Window.h>

#include <native.h>

#include "collection_view.h"
#include "../../control_render_access.h"
#include "globals.h"

namespace
{
    constexpr uint32 group_change_message = 0x6e746772;

    std::optional<native::table_group> group_for_row(
        native::table_model &model,
        std::size_t row,
        std::size_t *group_index = nullptr);

    class native_table_field final : public BField
    {
    public:
        native_table_field(std::string text,
                           const native::img *image,
            native::table_row_id row,
            std::size_t model_row,
            native::table_group_id group = 0)
            : row(row)
            , model_row(model_row)
            , group(group)
            , _text(std::move(text))
            , _image(image) {}

        const std::string &text() const {
            return _text;
        }

        const native::img *image() const {
            return _image;
        }

        native::table_row_id row;
        std::size_t model_row;
        native::table_group_id group;

    private:
        std::string _text;
        const native::img *_image;
    };

    class native_table_column final : public BTitledColumn
    {
    public:
        native_table_column(native::table_view &owner,
                            const native::table_column &column)
            : BTitledColumn(column.title.c_str(),
                            column.width,
                            column.min_width,
                            column.max_width,
                            alignment_for(column.alignment))
            , _owner(owner)
            , _column(column) {}

        void DrawField(BField *field,
                       BRect bounds,
                       BView *target) override {
            auto *value = dynamic_cast<native_table_field *>(field);
            if (!value || !target || !_owner.get_created())
                return;
            native::rect cell_bounds(
                static_cast<native::coord>(bounds.left),
                static_cast<native::coord>(bounds.top),
                static_cast<native::dim>(
                    std::max<float>(0, bounds.Width() + 1)),
                static_cast<native::dim>(
                    std::max<float>(0, bounds.Height() + 1)));
            native::gpx &graphics = _owner.get_gpx();
            graphics.set_clip(cell_bounds);
            auto appearance = native::theme::create(graphics);
            native::theme::state state;
            const auto &selection = _owner.get_selected_rows();
            state.selected = std::find(selection.begin(),
                                       selection.end(),
                                       value->row) != selection.end();
            state.focused = target->IsFocus();
            if (value->group != 0) {
                native::table_model *model = _owner.get_model();
                if (model) {
                    const auto group = group_for_row(
                        *model, value->model_row);
                    if (group) {
                        native::detail::control_render_access::
                            draw_table_group(
                                _owner,
                                graphics,
                                *appearance,
                                *group,
                                cell_bounds,
                                state);
                    }
                }
                return;
            }
            native::table_cell cell;
            cell.text = value->text();
            cell.image = value->image();
            native::detail::control_render_access::draw_table_cell(
                _owner,
                graphics,
                *appearance,
                value->row,
                value->model_row,
                _column,
                cell,
                cell_bounds,
                state);
        }

        int CompareFields(BField *left, BField *right) override {
            auto *first = dynamic_cast<native_table_field *>(left);
            auto *second = dynamic_cast<native_table_field *>(right);
            if (!first || !second)
                return 0;
            return std::strcmp(first->text().c_str(),
                               second->text().c_str());
        }

        bool AcceptsField(const BField *field) const override {
            return dynamic_cast<const native_table_field *>(field) !=
                   nullptr;
        }

    private:
        native::table_view &_owner;
        native::table_column _column;

        static alignment alignment_for(
            native::table_alignment value) {
            if (value == native::table_alignment::center)
                return B_ALIGN_CENTER;
            if (value == native::table_alignment::end)
                return B_ALIGN_RIGHT;
            return B_ALIGN_LEFT;
        }
    };

    class native_data_row : public BRow
    {
    public:
        native_data_row(native::table_row_id id, float height)
            : BRow(height)
            , id(id) {}

        native::table_row_id id;
    };

    class native_group_row final : public BRow
    {
    public:
        native_group_row(native::table_group_id id,
                         bool collapsible,
                         float height)
            : BRow(height)
            , id(id)
            , collapsible(collapsible) {}

        bool HasLatch() const override {
            return collapsible;
        }

        native::table_group_id id;
        bool collapsible;
    };

    class native_column_list final : public BColumnListView
    {
    public:
        native_column_list(native::table_view &owner,
                           BRect bounds,
                           bool horizontal_scrollbar)
            : BColumnListView(bounds,
                              "native_table",
                              B_FOLLOW_NONE,
                              B_WILL_DRAW | B_FRAME_EVENTS |
                                  B_NAVIGABLE,
                              B_FANCY_BORDER,
                              horizontal_scrollbar)
            , _owner(owner) {}

        void set_suppress(bool suppress) {
            _suppress = suppress;
        }

        void SelectionChanged() override {
            BColumnListView::SelectionChanged();
            if (_suppress || !_owner.get_created())
                return;
            std::vector<native::table_row_id> rows;
            BRow *selected = nullptr;
            while ((selected = CurrentSelection(selected))) {
                if (auto *row =
                        dynamic_cast<native_data_row *>(selected)) {
                    rows.push_back(row->id);
                }
            }
            _owner.on_native_selection(rows);
        }

        void ItemInvoked() override {
            BColumnListView::ItemInvoked();
            if (_suppress || !_owner.get_created())
                return;
            if (auto *row =
                    dynamic_cast<native_data_row *>(FocusRow())) {
                _owner.on_native_activate(row->id);
            }
        }

        void ExpandOrCollapse(BRow *row, bool expand) override {
            auto *group = dynamic_cast<native_group_row *>(row);
            if (group && !group->collapsible && !_suppress)
                return;
            BColumnListView::ExpandOrCollapse(row, expand);
            if (!group || _suppress || !_owner.get_created())
                return;
            BMessage message(group_change_message);
            message.AddUInt64("group", group->id);
            message.AddBool("expanded", expand);
            Window()->PostMessage(&message, this);
        }

        void MessageReceived(BMessage *message) override {
            if (message && message->what == group_change_message) {
                std::uint64_t group = 0;
                bool expanded = false;
                if (message->FindUInt64("group", &group) == B_OK &&
                    message->FindBool("expanded", &expanded) == B_OK) {
                    _owner.on_native_group_expand(group, expanded);
                }
                return;
            }
            BColumnListView::MessageReceived(message);
        }

        void KeyDown(const char *bytes, int32 count) override {
            if (bytes && count > 0 &&
                static_cast<unsigned char>(bytes[0]) >= 0x20 &&
                bytes[0] != '+' &&
                (modifiers() & (B_COMMAND_KEY | B_CONTROL_KEY)) == 0) {
                _owner.on_native_type_text(
                    std::string(bytes,
                                static_cast<std::size_t>(count)));
                return;
            }
            BColumnListView::KeyDown(bytes, count);
        }

    private:
        native::table_view &_owner;
        bool _suppress = false;
    };

    haiku::haiku_collection &binding_for(native::table_view &table) {
        auto *binding = haiku::table_view_bindings.object_from_handle(
            &table);
        if (!binding || !binding->view)
            throw std::runtime_error(
                "Haiku: missing table_view binding.");
        return *binding;
    }

    template <typename function_type>
    void with_locked_view(BView *view, function_type &&function) {
        BWindow *window = view ? view->Window() : nullptr;
        const bool locked = window && window->IsLocked();
        if (window && (locked || window->Lock())) {
            function();
            if (!locked)
                window->Unlock();
        }
    }

    std::optional<native::table_group> group_for_row(
        native::table_model &model,
        std::size_t row,
        std::size_t *group_index) {
        for (std::size_t index = 0;
             index < model.group_count(); ++index) {
            const native::table_group group = model.group(index);
            if (row >= group.first_row &&
                row < group.first_row + group.row_count) {
                if (group_index)
                    *group_index = index;
                return group;
            }
        }
        return std::nullopt;
    }

    void clear_rows(haiku::haiku_collection &binding) {
        if (binding.column_view)
            binding.column_view->Clear();
        binding.rows.clear();
        binding.row_ids.clear();
        binding.group_rows.clear();
        binding.group_ids.clear();
    }

    void clear_columns(BColumnListView &view) {
        while (view.CountColumns() > 0) {
            BColumn *column = view.ColumnAt(0);
            view.RemoveColumn(column);
            delete column;
        }
    }

    void rebuild_native(native::table_view &table) {
        auto &binding = binding_for(table);
        native_column_list &view = *static_cast<native_column_list *>(
            binding.column_view);
        with_locked_view(&view, [&] {
            view.set_suppress(true);
            clear_rows(binding);
            clear_columns(view);
            column_flags flags = B_ALLOW_COLUMN_NONE;
            if (table.get_columns_reorderable()) {
                flags = static_cast<column_flags>(
                    flags | B_ALLOW_COLUMN_MOVE);
            }
            if (table.get_columns_resizable()) {
                flags = static_cast<column_flags>(
                    flags | B_ALLOW_COLUMN_RESIZE);
            }
            if (table.get_column_visibility_menu_enabled()) {
                flags = static_cast<column_flags>(
                    flags | B_ALLOW_COLUMN_POPUP);
            }
            view.SetColumnFlags(flags);
            view.SetSelectionMode(
                table.get_selection_mode() ==
                        native::table_selection_mode::single
                    ? B_SINGLE_SELECTION_LIST
                    : B_MULTIPLE_SELECTION_LIST);
            view.SetSortingEnabled(false);
            const auto &columns = table.get_columns();
            for (std::size_t index = 0;
                 index < columns.size(); ++index) {
                auto *column = new native_table_column(
                    table, columns[index]);
                column->SetShowHeading(table.get_header_visible());
                view.AddColumn(column, static_cast<int32>(index));
                view.SetColumnVisible(column, columns[index].visible);
            }

            native::table_model *model = table.get_model();
            if (!model) {
                view.set_suppress(false);
                return;
            }
            const float row_height = static_cast<float>(
                table.get_row_height().value_or(20));
            for (std::size_t index = 0;
                 index < model->group_count(); ++index) {
                const native::table_group group = model->group(index);
                auto *row = new native_group_row(
                    group.id, group.collapsible, row_height);
                if (!columns.empty()) {
                    row->SetField(new native_table_field(
                                      group.title,
                                      nullptr,
                                      native::invalid_table_row_id,
                                      group.first_row,
                                      group.id),
                                  0);
                }
                view.AddRow(row);
                binding.group_rows.push_back(row);
                binding.group_ids.push_back(group.id);
            }
            for (std::size_t row_index = 0;
                 row_index < model->row_count(); ++row_index) {
                auto *row = new native_data_row(
                    model->row_id(row_index), row_height);
                for (std::size_t column = 0;
                     column < columns.size(); ++column) {
                    const native::table_cell cell = model->cell(
                        row_index, columns[column].id);
                    row->SetField(new native_table_field(
                                      cell.text,
                                      cell.image,
                                      row->id,
                                      row_index),
                                  static_cast<int32>(column));
                }
                std::size_t group_index = 0;
                const auto group = group_for_row(
                    *model, row_index, &group_index);
                view.AddRow(row,
                            group
                                ? binding.group_rows[group_index]
                                : nullptr);
                binding.rows.push_back(row);
                binding.row_ids.push_back(row->id);
            }
            for (std::size_t index = 0;
                 index < binding.group_rows.size(); ++index) {
                view.ExpandOrCollapse(
                    binding.group_rows[index],
                    table.get_group_expanded(
                        binding.group_ids[index]));
            }
            view.set_suppress(false);
        });
    }

    BView *create_native_table(native::table_view &table,
                               haiku::haiku_collection &binding) {
        BView *parent = haiku::parent_view(table.get_parent());
        BWindow *window = parent ? parent->Window() : nullptr;
        if (!parent || !window)
            throw std::runtime_error(
                "Haiku: table_view requires a created parent.");
        const native::rect bounds = table.get_bounds();
        const bool locked = window->IsLocked();
        if (!locked && !window->Lock())
            throw std::runtime_error(
                "Haiku: failed to lock table_view parent.");
        auto *view = new native_column_list(
            table,
            BRect(bounds.p.x,
                  bounds.p.y,
                  bounds.x2() - 1,
                  bounds.y2() - 1),
            table.get_horizontal_scrollbar_policy() !=
                native::scrollbar_policy::never);
        view->Hide();
        parent->AddChild(view);
        if (!locked)
            window->Unlock();
        binding.column_view = view;
        return view;
    }
} // namespace

namespace native
{
    void table_view::apply_table() {
        auto &binding = binding_for(*this);
        if (binding.native_table)
            rebuild_native(*this);
        else
            invalidate();
    }

    void table_view::apply_selection() {
        auto &binding = binding_for(*this);
        if (!binding.native_table) {
            invalidate();
            return;
        }
        auto &view = *static_cast<native_column_list *>(
            binding.column_view);
        with_locked_view(&view, [&] {
            view.set_suppress(true);
            view.DeselectAll();
            for (table_row_id id : _selection) {
                const auto found = std::find(binding.row_ids.begin(),
                                             binding.row_ids.end(),
                                             id);
                if (found != binding.row_ids.end()) {
                    view.AddToSelection(binding.rows[
                        static_cast<std::size_t>(
                            found - binding.row_ids.begin())]);
                }
            }
            view.set_suppress(false);
        });
    }

    void table_view::apply_scroll() {
        auto &binding = binding_for(*this);
        if (!binding.native_table ||
            _vertical_row >= get_display_row_count()) {
            invalidate();
            return;
        }
        const table_display_row display =
            get_display_row(_vertical_row);
        BRow *row = nullptr;
        if (display.group) {
            const auto found = std::find(
                binding.group_ids.begin(),
                binding.group_ids.end(),
                display.group_id);
            if (found != binding.group_ids.end()) {
                row = binding.group_rows[static_cast<std::size_t>(
                    found - binding.group_ids.begin())];
            }
        } else if (_model) {
            const table_row_id id = _model->row_id(display.model_row);
            const auto found = std::find(binding.row_ids.begin(),
                                         binding.row_ids.end(),
                                         id);
            if (found != binding.row_ids.end()) {
                row = binding.rows[static_cast<std::size_t>(
                    found - binding.row_ids.begin())];
            }
        }
        if (row) {
            with_locked_view(binding.column_view, [&] {
                binding.column_view->ScrollTo(row);
            });
        }
    }

    void table_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<table_view *>(this);
        auto *binding = new haiku::haiku_collection();
        binding->native_table =
            _data_mode == table_data_mode::materialized;
        if (binding->native_table) {
            binding->view = create_native_table(*self, *binding);
        } else {
            binding->view = haiku::create_collection_view(*self);
        }
        haiku::table_view_bindings.register_pair(self, binding);
        _created = true;
        self->synchronize_theme_metrics();
        if (binding->native_table) {
            rebuild_native(*self);
            self->apply_selection();
            self->apply_scroll();
        }
        self->on_native_create();
    }

    void table_view::show() const {
        auto *binding = haiku::table_view_bindings.object_from_handle(
            const_cast<table_view *>(this));
        if (!_created || !binding || !binding->view)
            throw std::runtime_error(
                "Haiku: table_view is not created.");
        BWindow *window = binding->view->Window();
        const bool locked = window && window->IsLocked();
        if (window && (locked || window->Lock())) {
            binding->view->Show();
            if (!locked)
                window->Unlock();
        }
    }

    void table_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<table_view *>(this);
        auto *binding =
            haiku::table_view_bindings.object_from_handle(self);
        self->on_native_destroy();
        if (binding) {
            if (binding->view) {
                BWindow *window = binding->view->Window();
                const bool locked = window && window->IsLocked();
                if (window && (locked || window->Lock())) {
                    if (binding->native_table)
                        clear_rows(*binding);
                    binding->view->RemoveSelf();
                    delete binding->view;
                    if (!locked)
                        window->Unlock();
                }
            }
            haiku::table_view_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
