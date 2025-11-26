#include "ReplClient.hpp"
#include "common/util/Log.hpp"
#include "third_party/fmt/include/fmt/format.h"

ReplClient::ReplClient(int port) : XSocketClient(port) {
    read_buffer_.resize(1024); // 1KB read buffer
}

void ReplClient::eval(const std::string& form) {
    if (!is_connected()) {
        lg::warn("[ReplClient] Not connected, cannot eval");
        return;
    }

    ReplMessageHeader header;
    header.length = static_cast<uint32_t>(form.length());
    header.type = ReplMessageType::EVAL;

    // ������� �����: ��������� + ������
    const char* header_ptr = reinterpret_cast<const char*>(&header);
    std::vector<char> buffer(header_ptr, header_ptr + sizeof(header));
    buffer.insert(buffer.end(), form.begin(), form.end());

    // ���������� ���������
    int result = write_to_socket(client_socket, buffer.data(), buffer.size());
    if (result == -1) {
        lg::error("[ReplClient] Failed to send message, disconnecting");
        disconnect();
    }
    else {
        lg::debug("[ReplClient] Sent message: {}", form);
    }
}

std::string ReplClient::read_response() {
    if (!is_connected()) {
        return "";
    }

    // ������ ����� (��������� - � ���������� ����� ������� ���������)
    int bytes_read = read_from_socket(client_socket, read_buffer_.data(), read_buffer_.size() - 1);

    if (bytes_read > 0) {
        read_buffer_[bytes_read] = '\0';
        std::string response(read_buffer_.data());
        lg::debug("[ReplClient] Received response: {}", response);
        return response;
    }
    else if (bytes_read == 0) {
        lg::info("[ReplClient] Server disconnected");
        disconnect();
    }
    else {
        lg::error("[ReplClient] Error reading response");
    }

    return "";
}