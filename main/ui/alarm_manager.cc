#include "alarm_manager.h"
#include "application.h"
#include <esp_log.h>
#include <cJSON.h>
#include <ctime>

#define TAG "AlarmManager"

AlarmManager::AlarmManager() {
    esp_timer_create_args_t args = {
        .callback = [](void* arg){ static_cast<AlarmManager*>(arg)->CheckAlarms(); },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "alarm_check_timer",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer_handle_));
    // kiểm tra lịch mỗi 60 giây
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 60 * 1000000));
}

AlarmManager::~AlarmManager() {
    if (timer_handle_) {
        esp_timer_stop(timer_handle_);
        esp_timer_delete(timer_handle_);
        timer_handle_ = nullptr;
    }
}

void AlarmManager::SetOnTriggered(std::function<void(const Alarm&)> cb) {
    on_triggered_ = cb;
}

void AlarmManager::AddAlarm(int hour, int minute,
                            const std::string& ringtone,
                            bool repeat_daily) {
    // clamp tham số an toàn, tách dòng để không dính -Wmisleading-indentation
    if (hour < 0) {
        hour = 0;
    }
    if (hour > 23) {
        hour = 23;
    }
    if (minute < 0) {
        minute = 0;
    }
    if (minute > 59) {
        minute = 59;
    }

    alarms_.push_back(Alarm{hour, minute, ringtone, repeat_daily});
    ESP_LOGI(TAG, "Added alarm %02d:%02d, ringtone=%s, repeat=%d",
             hour, minute, ringtone.c_str(), repeat_daily ? 1 : 0);
}

void AlarmManager::RemoveAll() {
    alarms_.clear();
    ESP_LOGI(TAG, "All alarms cleared");
}

void AlarmManager::ListAlarms(std::string& out_json) {
    cJSON* root = cJSON_CreateArray();
    for (auto& a : alarms_) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "hour", a.hour);
        cJSON_AddNumberToObject(item, "minute", a.minute);
        cJSON_AddStringToObject(item, "ringtone", a.ringtone.c_str());
        cJSON_AddBoolToObject(item, "repeat_daily", a.repeat_daily);
        cJSON_AddItemToArray(root, item);
    }
    char* s = cJSON_PrintUnformatted(root);
    out_json.assign(s ? s : "[]");
    if (s) cJSON_free(s);
    cJSON_Delete(root);
}

void AlarmManager::CheckAlarms() {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);

    for (auto it = alarms_.begin(); it != alarms_.end();) {
        if (ti.tm_hour == it->hour && ti.tm_min == it->minute) {
            TriggerAlarm(*it);
            if (!it->repeat_daily) {
                it = alarms_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void AlarmManager::TriggerAlarm(const Alarm& a) {
    ESP_LOGI(TAG, "Alarm triggered at %02d:%02d (%s)",
             a.hour, a.minute, a.ringtone.c_str());

    // Phát chuông qua Application (AudioService::PlaySound sử dụng assets theo tên)
    Application::GetInstance().Alert("Báo thức", "Đã đến giờ!", "bell",
                                     a.ringtone); // std::string -> std::string_view

    if (on_triggered_) on_triggered_(a);
}
