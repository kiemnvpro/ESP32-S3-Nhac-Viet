#pragma once
#include <string>
#include <vector>
#include <functional>
#include <esp_timer.h>

struct Alarm {
    int hour;               // 0..23
    int minute;             // 0..59
    std::string ringtone;   // ví dụ: "/spiffs/iphone.ogg"
    bool repeat_daily;      // lặp hằng ngày
};

class AlarmManager {
public:
    // Singleton
    static AlarmManager& GetInstance() {
        static AlarmManager instance;
        return instance;
    }

    void SetOnTriggered(std::function<void(const Alarm&)> cb);

    void AddAlarm(int hour, int minute,
                  const std::string& ringtone,
                  bool repeat_daily);

    void RemoveAll();

    void ListAlarms(std::string& out_json);

    // Tắt chuông đang reo (nếu còn)
    void StopRinging();
    bool IsRinging() const { return is_ringing_; }

private:
    AlarmManager();
    ~AlarmManager();
    AlarmManager(const AlarmManager&) = delete;
    AlarmManager& operator=(const AlarmManager&) = delete;

    void CheckAlarms();              // gọi mỗi phút
    void TriggerAlarm(const Alarm&); // tới giờ thì gọi

    // Điều khiển phát chuông lặp
    void StartRinging(const Alarm& a);
    void OnRingTimer();              // callback timer lặp chuông

    std::vector<Alarm> alarms_;

    esp_timer_handle_t timer_handle_ = nullptr;      // timer check phút
    esp_timer_handle_t ring_timer_handle_ = nullptr; // timer lặp chuông

    bool is_ringing_ = false;
    int  ring_count_ = 0;    // đã kêu được bao nhiêu lần
    Alarm current_alarm_{};

    std::function<void(const Alarm&)> on_triggered_;
};
