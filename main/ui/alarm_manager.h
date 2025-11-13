#pragma once
#include <string>
#include <vector>
#include <functional>
#include <esp_timer.h>

struct Alarm {
    int hour;               // 0..23
    int minute;             // 0..59
    std::string ringtone;   // tên asset .ogg: "ga.ogg" hoặc "iphone.ogg"
    bool repeat_daily;      // lặp hằng ngày
};

class AlarmManager {
public:
    static AlarmManager& GetInstance() {
        static AlarmManager instance;
        return instance;
    }

    void AddAlarm(int hour, int minute,
                  const std::string& ringtone = "ga.ogg",
                  bool repeat_daily = false);
    void RemoveAll();
    void ListAlarms(std::string& out_json);

    // nếu cần bắt sự kiện ngoài việc phát chuông
    void SetOnTriggered(std::function<void(const Alarm&)> cb);

private:
    AlarmManager();
    ~AlarmManager();
    AlarmManager(const AlarmManager&) = delete;
    AlarmManager& operator=(const AlarmManager&) = delete;

    void CheckAlarms();                 // gọi mỗi phút
    void TriggerAlarm(const Alarm& a);  // tới giờ thì gọi

    std::vector<Alarm> alarms_;
    esp_timer_handle_t timer_handle_ = nullptr;
    std::function<void(const Alarm&)> on_triggered_;
};
