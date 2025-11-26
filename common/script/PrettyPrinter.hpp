#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <cstdint>

#include "Object.hpp"
#include "Reader.hpp"

namespace script::pretty_print {

        // main pretty print function
        std::string to_string(const Object& obj, int line_length = 110);


        struct Node {
            enum class Kind : uint8_t { ATOM, LIST, IMPROPER_LIST, INVALID } kind = Kind::INVALID;
            enum class QuoteKind { QUOTE, UNQUOTE, QUASIQUOTE, UNQUOTE_SPLICING };

            std::vector<Node> child_nodes;
            std::string atom_str;
            std::vector<QuoteKind> quotes;

            Node* parent = nullptr;
            uint32_t my_depth = 0;
            uint32_t text_len = 0;
            bool break_list = false;
            uint8_t top_line_count = 0;
            uint8_t sub_elt_indent = 0;

            Node() = default;
            Node(const std::string& str) : kind(Kind::ATOM), atom_str(str) {}
            Node(std::vector<Node>&& list, bool is_list)
                : kind(is_list ? Kind::LIST : Kind::IMPROPER_LIST), child_nodes(std::move(list)) {
            }

            int get_quote_length() const;
            bool needs_end_paren_newline() const;
            void link(Node* this_parent, std::vector<Node*>* bfs_order, uint32_t depth);
        };

        Node to_node(const Object& obj);
        void recompute_lengths(const std::vector<Node*>& bfs_order);
        void break_list(Node* node);
        void insert_required_breaks(const std::vector<Node*>& bfs_order);
        int run_algorithm(const std::vector<Node*>& bfs_order, int line_length);
        std::string node_to_string(const Node* node);

} // namespace script::pretty_print