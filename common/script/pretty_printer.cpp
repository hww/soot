#include "pretty_printer.h"
#include <sstream>
#include <algorithm>

namespace script::pretty_print {



        // Вспомогательные функции
        inline const std::string quote_symbol(Node::QuoteKind kind) {
            switch (kind) {
            case Node::QuoteKind::QUOTE: return "'";
            case Node::QuoteKind::QUASIQUOTE: return "`";
            case Node::QuoteKind::UNQUOTE: return ",";
            case Node::QuoteKind::UNQUOTE_SPLICING: return ",@";
            default: ASSERT_NOT_REACHED(); return "[invalid]";
            }
        }

        int Node::get_quote_length() const {
            int out = 0;
            for (auto& q : quotes) {
                out += quote_symbol(q).length();
            }
            return out;
        }

        bool Node::needs_end_paren_newline() const {
            if (break_list) {
                return true;
            }
            if (!child_nodes.empty()) {
                return child_nodes.back().needs_end_paren_newline();
            }
            return false;
        }

        void Node::link(Node* this_parent, std::vector<Node*>* bfs_order, uint32_t depth) {
            parent = this_parent;
            my_depth = depth;
            bfs_order->push_back(this);
            switch (kind) {
            case Kind::ATOM:
                break;
            case Kind::LIST:
            case Kind::IMPROPER_LIST:
                ASSERT(!child_nodes.empty());
                for (auto& child : child_nodes) {
                    child.link(this, bfs_order, depth + 1);
                }
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }

        // Преобразование Object в Node
        Node to_node(const Object& obj) {
            switch (obj.type) {
            case ObjectType::EMPTY_LIST:
                return Node("()");

            case ObjectType::INTEGER:
            case ObjectType::FLOAT:
            case ObjectType::CHAR:
            case ObjectType::SYMBOL:
            case ObjectType::STRING:
                return Node(obj.print());

            case ObjectType::PAIR: {
                // Проверяем quoted формы
                auto first = obj.as_pair()->car;
                if (first.is_symbol() && first.as_symbol().name_ptr) {
                    std::string first_str = first.as_symbol().name_ptr;

                    if (first_str == "quote") {
                        auto second = obj.as_pair()->cdr;
                        if (second.is_pair() && second.as_pair()->cdr.is_empty_list()) {
                            Node result = to_node(second.as_pair()->car);
                            result.quotes.push_back(Node::QuoteKind::QUOTE);
                            return result;
                        }
                    }
                    else if (first_str == "unquote") {
                        auto second = obj.as_pair()->cdr;
                        if (second.is_pair() && second.as_pair()->cdr.is_empty_list()) {
                            Node result = to_node(second.as_pair()->car);
                            result.quotes.push_back(Node::QuoteKind::UNQUOTE);
                            return result;
                        }
                    }
                    else if (first_str == "quasiquote") {
                        auto second = obj.as_pair()->cdr;
                        if (second.is_pair() && second.as_pair()->cdr.is_empty_list()) {
                            Node result = to_node(second.as_pair()->car);
                            result.quotes.push_back(Node::QuoteKind::QUASIQUOTE);
                            return result;
                        }
                    }
                    else if (first_str == "unquote-splicing") {
                        auto second = obj.as_pair()->cdr;
                        if (second.is_pair() && second.as_pair()->cdr.is_empty_list()) {
                            Node result = to_node(second.as_pair()->car);
                            result.quotes.push_back(Node::QuoteKind::UNQUOTE_SPLICING);
                            return result;
                        }
                    }
                }

                // Обычный список или improper list
                std::vector<Node> children;
                Object to_print = obj;

                while (true) {
                    if (to_print.is_pair()) {
                        children.push_back(to_node(to_print.as_pair()->car));
                        to_print = to_print.as_pair()->cdr;
                        if (to_print.is_empty_list()) {
                            return Node(std::move(children), true);
                        }
                    }
                    else {
                        children.push_back(to_node(to_print));
                        return Node(std::move(children), false);
                    }
                }
            }

            case ObjectType::ARRAY: {
                std::vector<Node> children;
                auto elements = obj.as_vector();
                for (auto& elt : elements) {
                    children.push_back(to_node(elt));
                }
                return Node(std::move(children), true);
            }

            default:
                throw std::runtime_error("Unsupported object type for pretty printing: " +
                    object_type_to_string(obj.type));
            }
        }

        // Вычисление длин поддеревьев
        void recompute_lengths(const std::vector<Node*>& bfs_order) {
            for (auto it = bfs_order.rbegin(); it != bfs_order.rend(); it++) {
                Node* node = *it;
                switch (node->kind) {
                case Node::Kind::ATOM:
                    node->text_len = node->atom_str.length() + node->get_quote_length();
                    break;

                case Node::Kind::IMPROPER_LIST:
                case Node::Kind::LIST: {
                    if (node->break_list) {
                        // Специальный расчет для разбитых списков
                        int first_line_len = 1 + node->get_quote_length(); // открывающая скобка + quotes
                        int nodes_on_first_line = std::min(
                            static_cast<int>(node->child_nodes.size()),
                            static_cast<int>(node->top_line_count)
                        );

                        if (nodes_on_first_line > 0) {
                            for (int node_idx = 0; node_idx < nodes_on_first_line; node_idx++) {
                                first_line_len += node->child_nodes.at(node_idx).text_len;
                                first_line_len++; // пробел после элемента
                            }
                            first_line_len--; // убираем последний пробел
                        }

                        int max_line_len = first_line_len;

                        // Расчет для остальных строк
                        for (size_t node_idx = nodes_on_first_line; node_idx < node->child_nodes.size(); node_idx++) {
                            int line_len = node->sub_elt_indent + node->child_nodes.at(node_idx).text_len;
                            max_line_len = std::max(max_line_len, line_len);
                        }

                        node->text_len = max_line_len;
                    }
                    else {
                        // Обычный расчет для неразбитых списков
                        node->text_len = 1 + node->get_quote_length(); // открывающая скобка + quotes
                        for (auto& child : node->child_nodes) {
                            node->text_len += (child.text_len + 1); // элемент + пробел/скобка
                        }
                    }
                    break;
                }
                default:
                    ASSERT_NOT_REACHED();
                }
            }
        }

        // Разбиение списка
        void break_list(Node* node) {
            ASSERT(!node->break_list);
            node->break_list = true;
            node->sub_elt_indent = 2;
            node->top_line_count = 1;

            const std::unordered_set<std::string> sameline_splitters = {
                "if", "<", ">", "<=", ">=", "set!", "=", "!=", "+", "-", "*", "/",
                "the", "->", "and", "or", "logand", "logior", "logxor", "+!", "*!",
                "logtest?", "not", "zero?", "nonzero?"
            };

            if (!node->child_nodes.empty() && node->child_nodes[0].kind == Node::Kind::ATOM) {
                auto& name = node->child_nodes[0].atom_str;

                if (name == "defun" || name == "defun-debug" || name == "defbehavior" || name == "defstate") {
                    node->top_line_count = 3;
                }
                else if (name == "defmethod") {
                    node->top_line_count = 3;
                    if (node->child_nodes.size() >= 4 && node->child_nodes[2].kind == Node::Kind::ATOM) {
                        node->top_line_count = 4;
                    }
                }
                else if (name == "until" || name == "while" || name == "dotimes" || name == "countdown" ||
                    name == "when" || name == "behavior" || name == "lambda" || name == "define") {
                    node->top_line_count = 2;
                }
                else if (name == "let" || name == "let*" || name == "rlet") {
                    node->top_line_count = 2;
                    // Разбиваем определения переменных если нужно
                    if (node->child_nodes.size() > 1 && node->child_nodes[1].child_nodes.size() > 1 &&
                        !node->child_nodes[1].break_list) {
                        break_list(&node->child_nodes[1]);
                    }
                }
                else if (sameline_splitters.count(name) > 0) {
                    node->top_line_count = 2;
                    node->sub_elt_indent += name.size();
                }
                else if (name == "cond") {
                    // Разбиваем все ветки cond
                    for (size_t i = 1; i < node->child_nodes.size(); i++) {
                        auto& cond_body = node->child_nodes[i];
                        if (cond_body.kind == Node::Kind::LIST && !cond_body.break_list) {
                            break_list(&cond_body);
                        }
                    }
                }
                else if (name == "case") {
                    node->top_line_count = 2;
                    // Разбиваем все ветки case
                    for (size_t i = 2; i < node->child_nodes.size(); i++) {
                        auto& case_body = node->child_nodes[i];
                        if (case_body.kind == Node::Kind::LIST && !case_body.break_list) {
                            break_list(&case_body);
                        }
                    }
                }
            }
            else if (!node->child_nodes.empty() && node->child_nodes[0].kind == Node::Kind::LIST) {
                node->sub_elt_indent = 1;
            }

            // Поднимаемся вверх по родителям, разбивая при необходимости
            Node* child = node;
            for (Node* p = node->parent; p; p = p->parent) {
                if (!p->break_list && &p->child_nodes.back() != child) {
                    break_list(p);
                }
                child = p;
            }
        }

        // Обязательное разбиение для определенных форм
        void insert_required_breaks(const std::vector<Node*>& bfs_order) {
            const std::unordered_set<std::string> always_break = {
                "when", "defun-debug", "countdown", "case", "defun", "defmethod",
                "let", "until", "while", "if", "dotimes", "cond", "else",
                "defbehavior", "rlet", "defstate", "behavior", "loop", "let*"
            };

            for (auto node : bfs_order) {
                if (!node->break_list && node->kind == Node::Kind::LIST &&
                    node->child_nodes.size() > 0 && node->child_nodes[0].kind == Node::Kind::ATOM) {
                    if (always_break.count(node->child_nodes[0].atom_str) > 0) {
                        break_list(node);
                    }
                }
            }
        }

        // Основной алгоритм разбиения
        int run_algorithm(const std::vector<Node*>& bfs_order, int line_length) {
            int num_broken = 0;
            std::optional<int32_t> min_depth;

            for (auto it = bfs_order.rbegin(); it != bfs_order.rend(); it++) {
                Node* node = *it;
                if (min_depth && node->my_depth < min_depth) {
                    break;
                }

                if (node->kind != Node::Kind::ATOM &&
                    static_cast<int>(node->text_len) > line_length &&
                    !node->break_list) {
                    break_list(node);
                    num_broken++;
                    if (!min_depth) {
                        min_depth = node->my_depth;
                    }
                }
            }

            recompute_lengths(bfs_order);
            return num_broken;
        }

        // Вспомогательная функция для вычисления отступов
        int compute_extra_offset(const std::string& str, int s0, int ei) {
            ASSERT(!str.empty());
            for (size_t i = str.length(); i-- > 0;) {
                if (static_cast<int>(i) == s0) {
                    return ei + static_cast<int>(str.length()) - s0;
                }
                else if (str[i] == '\n') {
                    return static_cast<int>(str.length()) - static_cast<int>(i);
                }
            }
            return ei + static_cast<int>(str.length()) - s0;
        }

        // Рекурсивная генерация строки
        void append_node_to_string(const Node* node, std::string& str,
            int init_indent_level, int next_indent_level) {
            // Начальный отступ
            for (int i = 0; i < init_indent_level; i++) {
                str.push_back(' ');
            }

            // Quotes
            for (auto q : node->quotes) {
                str.append(quote_symbol(q));
            }

            switch (node->kind) {
            case Node::Kind::ATOM:
                str.append(node->atom_str);
                break;

            case Node::Kind::IMPROPER_LIST:
            case Node::Kind::LIST:
                if (node->break_list) {
                    str.push_back('(');
                    size_t node_idx = 0;

                    int listing_indent = next_indent_level + node->get_quote_length() + node->sub_elt_indent;
                    int extra_indent = 0;
                    int old_indent = listing_indent;

                    if (node->top_line_count > 0) {
                        listing_indent -= node->sub_elt_indent;
                        listing_indent += (node->child_nodes.front().kind == Node::Kind::LIST) ? 1 : 2;
                    }

                    // Элементы на первой строке
                    for (; node_idx < node->top_line_count && node_idx < node->child_nodes.size(); node_idx++) {
                        size_t s0 = str.length();
                        if (node->kind == Node::Kind::IMPROPER_LIST &&
                            &node->child_nodes.at(node_idx) == &node->child_nodes.back()) {
                            str.append(". ");
                        }

                        append_node_to_string(&node->child_nodes.at(node_idx), str, 0,
                            listing_indent + extra_indent);
                        extra_indent = compute_extra_offset(str, static_cast<int>(s0), extra_indent);
                        str.push_back(' ');
                    }

                    if (node->top_line_count > 0) {
                        listing_indent = old_indent;
                    }

                    if (node->top_line_count > 0 && node_idx > 0) {
                        str.pop_back(); // убираем последний пробел
                    }

                    if (node_idx < node->child_nodes.size()) {
                        str.push_back('\n');
                    }

                    // Остальные элементы на новых строках
                    bool after_key = false;
                    for (; node_idx < node->child_nodes.size(); node_idx++) {
                        if (node->kind == Node::Kind::IMPROPER_LIST &&
                            &node->child_nodes.at(node_idx) == &node->child_nodes.back()) {
                            for (int i = 0; i < listing_indent; i++) {
                                str.push_back(' ');
                            }
                            str.append(".\n");
                        }

                        append_node_to_string(&node->child_nodes.at(node_idx), str,
                            after_key ? 0 : listing_indent, listing_indent);

                        // Специальная обработка для keyword аргументов
                        if (node->child_nodes.at(node_idx).kind == Node::Kind::ATOM &&
                            node->child_nodes.at(node_idx).atom_str.length() > 0 &&
                            node->child_nodes.at(node_idx).atom_str[0] == ':' &&
                            node->child_nodes.at(node_idx).atom_str.find(' ') == std::string::npos) {
                            str.push_back(' ');
                            after_key = true;
                        }
                        else {
                            if (node_idx < node->child_nodes.size() - 1) {
                                str.push_back('\n');
                            }
                            after_key = false;
                        }
                    }

                    // Закрывающая скобка
                    if (!node->child_nodes.empty()) {
                        str.push_back('\n');
                        for (int i = 0; i < next_indent_level + node->get_quote_length(); i++) {
                            str.push_back(' ');
                        }
                    }
                    str.push_back(')');

                }
                else {
                    // Неразбитый список
                    str.push_back('(');
                    ASSERT(!node->child_nodes.empty());

                    int listing_indent = next_indent_level + node->get_quote_length();
                    int extra_indent = 1;

                    for (size_t i = 0; i < node->child_nodes.size(); i++) {
                        auto& child = node->child_nodes[i];

                        if (node->kind == Node::Kind::IMPROPER_LIST && i == node->child_nodes.size() - 1) {
                            str.append(". ");
                        }

                        size_t s0 = str.length();
                        append_node_to_string(&child, str, 0, listing_indent + extra_indent);

                        if (i < node->child_nodes.size() - 1) {
                            str.push_back(' ');
                        }
                        extra_indent = compute_extra_offset(str, static_cast<int>(s0), extra_indent);
                    }

                    if (node->needs_end_paren_newline()) {
                        str.push_back('\n');
                        for (int i = 0; i < listing_indent; i++) {
                            str.push_back(' ');
                        }
                    }
                    str.push_back(')');
                }
                break;

            default:
                ASSERT_NOT_REACHED();
            }
        }

        std::string node_to_string(const Node* node) {
            std::string result;
            append_node_to_string(node, result, 0, 0);
            return result;
        }

        // Главная функция pretty printer'а
        std::string to_string(const Object& obj, int line_length) {
            // Строим дерево
            Node root = to_node(obj);

            // Создаем связи и порядок обхода
            std::vector<Node*> bfs_order;
            root.link(nullptr, &bfs_order, 0);

            // Обязательное разбиение для некоторых форм
            insert_required_breaks(bfs_order);

            // Вычисляем длины поддеревьев
            recompute_lengths(bfs_order);

            // Итеративное разбиение пока нужно
            int num_broken = 1;
            while (num_broken > 0) {
                num_broken = run_algorithm(bfs_order, line_length);
            }

            return node_to_string(&root);
        }

} // namespace script::pretty_print