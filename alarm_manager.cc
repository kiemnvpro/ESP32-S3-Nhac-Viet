#include "alarm_manager.h"
#include "application.h"
#include "assets/lang_config.h"   // để dùng Lang::Sounds::OGG_ACTIVATION
#include <esp_log.h>
#include <cJSON.h>
#include <ctime>

#define TAG "AlarmManager"

AlarmManager::AlarmManager() {
    // Timer kiểm tra báo thức mỗi phút
    esp_timer_create_args_t args = {
        .callback = [](void* arg) {
            static_cast<AlarmManager*>(arg)->CheckAlarms();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "alarm_check_timer",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer_handle_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 60 * 1000000));

    // Timer lặp chuông
    esp_timer_create_args_t ring_args = {
        .callback = [](void* arg) {
            static_cast<AlarmManager*>(arg)->OnRingTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "alarm_ring_timer",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&ring_args, &ring_timer_handle_));
}

AlarmManager::~AlarmManager() {
    if (timer_handle_) {
        esp_timer_stop(timer_handle_);
        esp_timer_delete(timer_handle_);
        timer_handle_ = nullptr;
    }
    if (ring_timer_handle_) {
        esp_timer_stop(ring_timer_handle_);
        esp_timer_delete(ring_timer_handle_);
        ring_timer_handle_ = nullptr;
    }
}

void AlarmManager::SetOnTriggered(std::function<void(const Alarm&)> cb) {
    on_triggered_ = cb;
}

void AlarmManager::AddAlarm(int hour, int minute,
                            const std::string& ringtone,
                            bool repeat_daily) {
    if (hour   < 0) hour   = 0;
    if (hour   > 23) hour  = 23;
    if (minute < 0) minute = 0;
    if (minute > 59) minute = 59;

    alarms_.push_back(Alarm{hour, minute, ringtone, repeat_daily});
    ESP_LOGI(TAG, "Added alarm %02d:%02d, ringtone=%s, repeat=%d",
             hour, minute, ringtone.c_str(), repeat_daily ? 1 : 0);
}

void AlarmManager::RemoveAll() {
    // nếu đang reo thì tắt luôn
    StopRinging();
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
    ESP_LOGI(TAG, "Alarm triggered at %02d:%02d (ring=%s)",
             a.hour, a.minute, a.ringtone.c_str());

    StartRinging(a);

    if (on_triggered_) {
        on_triggered_(a);
    }
}

// Bắt đầu chế độ reo lặp: tổng cộng 5 lần, cách nhau 3 giây
void AlarmManager::StartRinging(const Alarm& a) {
    current_alarm_ = a;

    // reset trạng thái
    ring_count_ = 0;
    is_ringing_ = true;

    // nếu timer cũ còn chạy thì dừng lại
    if (ring_timer_handle_) {
        esp_timer_stop(ring_timer_handle_);
    }

    ESP_LOGI(TAG, "Start ringing alarm loop at %02d:%02d", a.hour, a.minute);

    // Lần đầu tiên: kêu ngay lập tức
    Application::GetInstance().Alert(
        "Báo thức",
        "Đã đến giờ!",
        "bell",
        Lang::Sounds::OGG_ACTIVATION   // âm OGG mà anh nói đã kêu OK
    );
    ring_count_ = 1;

    // Sau đó 3 giây thì kêu lần 2,3,4,5...
    if (ring_timer_handle_) {
        // 3 giây = 3 * 1_000_000 microseconds
        ESP_ERROR_CHECK(esp_timer_start_periodic(ring_timer_handle_, 9 * 1000000));
    }
}

void AlarmManager::OnRingTimer() {
    if (!is_ringing_) {
        return;
    }

    // Nếu đã kêu đủ 5 lần (bao gồm cả lần đầu tiên) thì dừng
    if (ring_count_ >= 5) {
        ESP_LOGI(TAG, "Alarm reached max ring count, stopping");
        StopRinging();
        return;
    }

    ESP_LOGI(TAG, "Alarm ring tick #%d", ring_count_ + 1);

    // Phát lại chuông
    Application::GetInstance().Alert(
        "Báo thức",
        "Đã đến giờ!",
        "bell",
        Lang::Sounds::OGG_ACTIVATION
    );
    ring_count_++;
}

// Có thể gọi qua tool self.alarm.stop hoặc khi clear báo thức
void AlarmManager::StopRinging() {
    if (!is_ringing_) {
        return;
    }

    is_ringing_ = false;
    ring_count_ = 0;

    if (ring_timer_handle_) {
        esp_timer_stop(ring_timer_handle_);
    }

    ESP_LOGI(TAG, "Alarm ringing stopped");
}
