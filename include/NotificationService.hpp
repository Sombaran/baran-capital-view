#ifndef FOLIO_NOTIFICATION_SERVICE_HPP
#define FOLIO_NOTIFICATION_SERVICE_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <ctime>

namespace folio {

/**
 * @brief Multi-channel push notification service.
 * 
 * Dispatches real-time alerts via:
 * - WebSocket to browser clients (low-latency)
 * - HTTP webhooks (Slack, Discord, IFTTT, custom APIs)
 * - Firebase Cloud Messaging (mobile apps)
 * - Email (SMTP)
 * - SMS (Twilio, AWS SNS)
 * 
 * Notifications support templating and recipient filtering.
 * 
 * v2.0.0+: Keeps users informed of trading signals and portfolio changes.
 */
class NotificationService {
public:
    enum class AlertType {
        SIGNAL_GENERATED,       // Buy/Sell opportunity
        PRICE_ALERT,            // Breakout/breakdown
        PORTFOLIO_ALERT,        // Health score change
        NEWS_ALERT,             // Important news
        ORDER_EXECUTED,         // Order filled
        ORDER_CANCELLED,        // Order cancelled
        RISK_WARNING            // Risk threshold breached
    };

    enum class Channel {
        WEBSOCKET,              // Browser/client WebSocket
        WEBHOOK,                // HTTP webhook (Slack, Discord, etc.)
        FIREBASE,               // Firebase Cloud Messaging (FCM)
        EMAIL,                  // SMTP email
        SMS                     // SMS (Twilio, AWS SNS, etc.)
    };

    struct Notification {
        std::string id;
        AlertType type = AlertType::SIGNAL_GENERATED;
        
        // Message content
        std::string title;
        std::string message;
        std::string details;     // Structured JSON details
        
        // Recipient
        std::string recipient;   // Email, phone, device token, etc.
        std::vector<Channel> channels;
        
        // Timing
        time_t createdTime = 0;
        time_t sentTime = 0;
        
        // Status
        bool sent = false;
        std::string error;
        
        // Metadata
        std::string symbolsInvolved;  // CSV of symbols
        double severity = 0.5;        // 0–1 scale
    };

    struct NotificationConfig {
        // Channel enablement
        bool enableWebSocket = true;
        bool enableWebhooks = true;
        bool enableFirebase = false;
        bool enableEmail = false;
        bool enableSMS = false;
        
        // Rate limiting
        int maxNotificationsPerHour = 100;
        int maxNotificationsPerSymbolPerDay = 10;
        
        // Severity thresholds
        double minSeverityForAlert = 0.3;  // Don't notify below this
        
        // Message formatting
        bool useRichText = true;  // HTML/Markdown
        bool includeCharts = false;  // Embed mini-charts
        
        // Timing
        bool quietHoursEnabled = true;
        int quietHoursStart = 22;   // 10 PM
        int quietHoursEnd = 8;      // 8 AM
        
        // Template customization
        std::map<std::string, std::string> templateOverrides;
    };

    struct WebhookEndpoint {
        std::string name;
        std::string url;
        std::string authToken;  // For bearer auth
        std::vector<AlertType> subscribedTypes;
        bool active = true;
    };

    struct FirebaseConfig {
        std::string serverKey;
        std::vector<std::string> registeredDeviceTokens;
    };

    struct EmailConfig {
        std::string smtpServer;
        int smtpPort = 587;
        bool useTls = true;
        std::string senderEmail;
        std::string senderPassword;
        std::vector<std::string> recipientEmails;
    };

    struct SmsConfig {
        std::string provider;  // "twilio", "sns", "custom"
        std::string accountId;
        std::string authToken;
        std::vector<std::string> recipientPhones;
    };

    /**
     * @brief Constructor with configuration.
     */
    explicit NotificationService(const NotificationConfig& config);

    /**
     * @brief Send a notification.
     * @param notification Notification object to send.
     * @return Notification ID if queued successfully.
     */
    std::string send(Notification& notification);

    /**
     * @brief Send notification with convenience parameters.
     */
    std::string sendAlert(AlertType type,
                          const std::string& title,
                          const std::string& message,
                          const std::string& details = "",
                          double severity = 0.5);

    /**
     * @brief Register notification callback (for WebSocket clients).
     * Called when a notification is ready to send via WebSocket.
     */
    using NotificationCallback = std::function<void(const Notification&)>;
    void setNotificationCallback(NotificationCallback callback);

    /**
     * @brief Add webhook endpoint.
     * Webhooks receive POST requests with notification JSON.
     */
    void addWebhookEndpoint(const WebhookEndpoint& endpoint);

    /**
     * @brief Remove webhook endpoint.
     */
    void removeWebhookEndpoint(const std::string& name);

    /**
     * @brief Configure Firebase Cloud Messaging.
     */
    void setFirebaseConfig(const FirebaseConfig& config);

    /**
     * @brief Register a mobile device for push notifications.
     * @param deviceToken Firebase registration token.
     */
    void registerDevice(const std::string& deviceToken);

    /**
     * @brief Unregister a device.
     */
    void unregisterDevice(const std::string& deviceToken);

    /**
     * @brief Configure email notifications.
     */
    void setEmailConfig(const EmailConfig& config);

    /**
     * @brief Configure SMS notifications.
     */
    void setSmsConfig(const SmsConfig& config);

    /**
     * @brief Get notification status.
     */
    Notification getNotificationStatus(const std::string& notificationId);

    /**
     * @brief Get recent notifications (for web UI history).
     */
    std::vector<Notification> getRecentNotifications(int limit = 50);

    /**
     * @brief Clear notification history.
     */
    void clearHistory();

    /**
     * @brief Update configuration.
     */
    void setConfig(const NotificationConfig& config);

    /**
     * @brief Get current configuration.
     */
    NotificationConfig getConfig() const;

    /**
     * @brief Test notification delivery (sends test message).
     */
    bool testDelivery(Channel channel);

private:
    NotificationConfig config_;
    NotificationCallback callback_;
    
    std::vector<Notification> history_;
    std::map<std::string, int> symbolNotificationCount_;
    
    std::vector<WebhookEndpoint> webhookEndpoints_;
    FirebaseConfig firebaseConfig_;
    EmailConfig emailConfig_;
    SmsConfig smsConfig_;

    // Helper methods
    bool passesRateLimit(AlertType type, const std::string& symbol);
    bool isInQuietHours() const;
    std::string generateNotificationId() const;
    std::string renderTemplate(AlertType type, const Notification& notification);
    
    bool sendViaWebSocket(const Notification& notification);
    bool sendViaWebhook(const Notification& notification, const WebhookEndpoint& endpoint);
    bool sendViaFirebase(const Notification& notification);
    bool sendViaEmail(const Notification& notification);
    bool sendViaSMS(const Notification& notification);
};

}  // namespace folio

#endif  // FOLIO_NOTIFICATION_SERVICE_HPP
