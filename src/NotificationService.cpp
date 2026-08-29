#include "NotificationService.hpp"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>

#ifdef HAVE_UUID_H
#include <uuid/uuid.h>
#endif

namespace folio {

NotificationService::NotificationService(const NotificationConfig& config)
    : config_(config) {
}

std::string NotificationService::send(Notification& notification) {
    notification.id = generateNotificationId();
    notification.createdTime = time(nullptr);

    // Check rate limits
    if (!passesRateLimit(notification.type, notification.symbolsInvolved)) {
        notification.error = "Rate limit exceeded";
        return notification.id;
    }

    // Check quiet hours
    if (config_.quietHoursEnabled && isInQuietHours()) {
        notification.error = "Quiet hours active";
        return notification.id;
    }

    // Check severity threshold
    if (notification.severity < config_.minSeverityForAlert) {
        notification.error = "Severity below threshold";
        return notification.id;
    }

    // Send via enabled channels
    bool sentAny = false;

    if (config_.enableWebSocket && callback_) {
        if (sendViaWebSocket(notification)) {
            sentAny = true;
        }
    }

    if (config_.enableWebhooks) {
        for (const auto& endpoint : webhookEndpoints_) {
            if (std::find(endpoint.subscribedTypes.begin(),
                         endpoint.subscribedTypes.end(),
                         notification.type) != endpoint.subscribedTypes.end()) {
                if (sendViaWebhook(notification, endpoint)) {
                    sentAny = true;
                }
            }
        }
    }

    if (config_.enableFirebase) {
        if (sendViaFirebase(notification)) {
            sentAny = true;
        }
    }

    if (config_.enableEmail) {
        if (sendViaEmail(notification)) {
            sentAny = true;
        }
    }

    if (config_.enableSMS) {
        if (sendViaSMS(notification)) {
            sentAny = true;
        }
    }

    notification.sent = sentAny;
    notification.sentTime = time(nullptr);

    // Add to history
    history_.push_back(notification);

    return notification.id;
}

std::string NotificationService::sendAlert(AlertType type,
                                            const std::string& title,
                                            const std::string& message,
                                            const std::string& details,
                                            double severity) {
    Notification notif;
    notif.type = type;
    notif.title = title;
    notif.message = message;
    notif.details = details;
    notif.severity = severity;

    return send(notif);
}

void NotificationService::setNotificationCallback(NotificationCallback callback) {
    callback_ = callback;
}

void NotificationService::addWebhookEndpoint(const WebhookEndpoint& endpoint) {
    webhookEndpoints_.push_back(endpoint);
}

void NotificationService::removeWebhookEndpoint(const std::string& name) {
    auto it = std::remove_if(webhookEndpoints_.begin(),
                             webhookEndpoints_.end(),
                             [&name](const WebhookEndpoint& e) { return e.name == name; });
    webhookEndpoints_.erase(it, webhookEndpoints_.end());
}

void NotificationService::setFirebaseConfig(const FirebaseConfig& config) {
    firebaseConfig_ = config;
}

void NotificationService::registerDevice(const std::string& deviceToken) {
    firebaseConfig_.registeredDeviceTokens.push_back(deviceToken);
}

void NotificationService::unregisterDevice(const std::string& deviceToken) {
    auto it = std::find(firebaseConfig_.registeredDeviceTokens.begin(),
                       firebaseConfig_.registeredDeviceTokens.end(),
                       deviceToken);
    if (it != firebaseConfig_.registeredDeviceTokens.end()) {
        firebaseConfig_.registeredDeviceTokens.erase(it);
    }
}

void NotificationService::setEmailConfig(const EmailConfig& config) {
    emailConfig_ = config;
}

void NotificationService::setSmsConfig(const SmsConfig& config) {
    smsConfig_ = config;
}

NotificationService::Notification NotificationService::getNotificationStatus(
    const std::string& notificationId) {
    for (const auto& notif : history_) {
        if (notif.id == notificationId) {
            return notif;
        }
    }
    return Notification();
}

std::vector<NotificationService::Notification> NotificationService::getRecentNotifications(
    int limit) {
    std::vector<Notification> recent;
    int count = 0;

    for (auto it = history_.rbegin(); it != history_.rend() && count < limit; ++it, ++count) {
        recent.push_back(*it);
    }

    return recent;
}

void NotificationService::clearHistory() {
    history_.clear();
    symbolNotificationCount_.clear();
}

void NotificationService::setConfig(const NotificationConfig& config) {
    config_ = config;
}

NotificationService::NotificationConfig NotificationService::getConfig() const {
    return config_;
}

bool NotificationService::testDelivery(Channel channel) {
    Notification testNotif;
    testNotif.title = "Test Notification";
    testNotif.message = "This is a test notification from Portfolio Health";
    testNotif.severity = 0.5;

    switch (channel) {
        case Channel::WEBSOCKET:
            return sendViaWebSocket(testNotif);
        case Channel::WEBHOOK:
            if (!webhookEndpoints_.empty()) {
                return sendViaWebhook(testNotif, webhookEndpoints_[0]);
            }
            return false;
        case Channel::FIREBASE:
            return sendViaFirebase(testNotif);
        case Channel::EMAIL:
            return sendViaEmail(testNotif);
        case Channel::SMS:
            return sendViaSMS(testNotif);
    }

    return false;
}

bool NotificationService::passesRateLimit(AlertType type, const std::string& symbol) {
    // Check hourly limit
    int hourlyCount = 0;
    time_t oneHourAgo = time(nullptr) - 3600;

    for (const auto& notif : history_) {
        if (notif.createdTime > oneHourAgo) {
            hourlyCount++;
        }
    }

    if (hourlyCount >= config_.maxNotificationsPerHour) {
        return false;
    }

    // Check per-symbol daily limit
    time_t oneDayAgo = time(nullptr) - 86400;
    int symbolCount = 0;

    for (const auto& notif : history_) {
        if (notif.createdTime > oneDayAgo &&
            notif.symbolsInvolved == symbol) {
            symbolCount++;
        }
    }

    return symbolCount < config_.maxNotificationsPerSymbolPerDay;
}

bool NotificationService::isInQuietHours() const {
    if (!config_.quietHoursEnabled) {
        return false;
    }

    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    int hour = timeinfo->tm_hour;

    if (config_.quietHoursStart < config_.quietHoursEnd) {
        // Normal range (e.g., 10 PM to 8 AM would be 22 to 8)
        return hour >= config_.quietHoursStart || hour < config_.quietHoursEnd;
    } else {
        // Wrapped range
        return hour >= config_.quietHoursStart || hour < config_.quietHoursEnd;
    }
}

std::string NotificationService::generateNotificationId() const {
#ifdef HAVE_UUID_H
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
#else
    // Fallback UUID generation using timestamp + random
    // Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx (v4 style)
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    time_t now = time(nullptr);
    
    // Use timestamp and random numbers for UUID-like string
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; ++i) {
        ss << std::hex << dis(gen);
    }
    ss << "-4";  // Version 4
    for (int i = 0; i < 3; ++i) {
        ss << std::hex << dis(gen);
    }
    ss << "-";
    ss << std::hex << (dis(gen) & 0x3 | 0x8);  // Variant bits
    for (int i = 0; i < 3; ++i) {
        ss << std::hex << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
#endif
}

std::string NotificationService::renderTemplate(AlertType type,
                                                 const Notification& notification) {
    // Basic template rendering
    std::string template_str = notification.title + "\n" + notification.message;
    
    if (!notification.details.empty()) {
        template_str += "\n" + notification.details;
    }

    return template_str;
}

bool NotificationService::sendViaWebSocket(const Notification& notification) {
    if (!callback_) {
        return false;
    }

    // Invoke callback which sends to connected WebSocket clients
    callback_(notification);
    return true;
}

bool NotificationService::sendViaWebhook(const Notification& notification,
                                          const WebhookEndpoint& endpoint) {
    if (!endpoint.active) {
        return false;
    }

    // TODO: Make HTTP POST request to endpoint.url with notification JSON
    // Use libcurl or similar to send the webhook payload
    return true;
}

bool NotificationService::sendViaFirebase(const Notification& notification) {
    if (firebaseConfig_.registeredDeviceTokens.empty() ||
        firebaseConfig_.serverKey.empty()) {
        return false;
    }

    // TODO: Call Firebase Cloud Messaging API
    // POST to https://fcm.googleapis.com/fcm/send with server key and device tokens
    return true;
}

bool NotificationService::sendViaEmail(const Notification& notification) {
    if (emailConfig_.recipientEmails.empty() ||
        emailConfig_.senderEmail.empty()) {
        return false;
    }

    // TODO: Connect to SMTP server and send email
    // Use libcurl or similar for SMTP protocol
    return true;
}

bool NotificationService::sendViaSMS(const Notification& notification) {
    if (smsConfig_.recipientPhones.empty() ||
        smsConfig_.provider.empty()) {
        return false;
    }

    // TODO: Call SMS provider API (Twilio, AWS SNS, etc.)
    return true;
}

}  // namespace folio
