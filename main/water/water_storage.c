#include "water_storage.h"

#include <string.h>

#include "esp_log.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "rom/crc.h"

static const char *TAG = "water_store";

#define WATERLOG_PARTITION_LABEL   "waterlog"
#define WATERLOG_PARTITION_SUBTYPE 0x40

#define WATER_HEADER_MAGIC  0x57415445U /* "WATE" */
#define WATER_HEADER_VERSION 1U
#define WATER_RECORD_MAGIC  0xF1D0U

#define WATER_HEADER_SECTOR_SIZE 4096U
#define WATER_RECORD_SIZE        128U
#define WATER_RECORD_CAPACITY    WATER_METRICS_MAX_ENTRIES

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t next_write;
    uint16_t count;
    uint16_t reserved;
    uint32_t write_gen;
    uint32_t header_crc;
} water_header_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t valid_flags;
    uint8_t reserved;
    int64_t timestamp_unix;
    float ph;
    float ammonia_ppm;
    float nitrite_ppm;
    float nitrate_ppm;
    char notes[WATER_TEST_NOTES_LEN];
    uint32_t record_crc;
} water_record_t;

_Static_assert(sizeof(water_record_t) == WATER_RECORD_SIZE, "water_record_t must be 128 bytes");

static const esp_partition_t *s_part;
static SemaphoreHandle_t s_mutex;
static bool s_inited;

static uint32_t header_crc(const water_header_t *hdr)
{
    water_header_t tmp = *hdr;
    tmp.header_crc = 0;
    return crc32_le(0, (const uint8_t *)&tmp, sizeof(tmp));
}

static uint32_t record_crc(const water_record_t *rec)
{
    water_record_t tmp = *rec;
    tmp.record_crc = 0;
    return crc32_le(0, (const uint8_t *)&tmp, sizeof(tmp));
}

static esp_err_t read_header(water_header_t *hdr)
{
    return esp_partition_read(s_part, 0, hdr, sizeof(*hdr));
}

static esp_err_t write_header(const water_header_t *hdr)
{
    static uint8_t sector[WATER_HEADER_SECTOR_SIZE];

    memset(sector, 0xFF, sizeof(sector));
    memcpy(sector, hdr, sizeof(*hdr));

    esp_err_t err = esp_partition_erase_range(s_part, 0, WATER_HEADER_SECTOR_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase water header sector: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_partition_write(s_part, 0, sector, sizeof(sector));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write water header: %s", esp_err_to_name(err));
    }
    return err;
}

static uint32_t record_offset(uint16_t index)
{
    return WATER_HEADER_SECTOR_SIZE + (uint32_t)index * WATER_RECORD_SIZE;
}

static void record_to_entry(const water_record_t *rec, water_test_entry_t *entry)
{
    memset(entry, 0, sizeof(*entry));
    entry->timestamp_unix = rec->timestamp_unix;
    entry->ph = rec->ph;
    entry->ammonia_ppm = rec->ammonia_ppm;
    entry->nitrite_ppm = rec->nitrite_ppm;
    entry->nitrate_ppm = rec->nitrate_ppm;
    entry->valid_flags = rec->valid_flags;
    memcpy(entry->notes, rec->notes, WATER_TEST_NOTES_LEN);
    entry->notes[WATER_TEST_NOTES_LEN - 1] = '\0';
}

static void entry_to_record(const water_test_entry_t *entry, water_record_t *rec)
{
    memset(rec, 0, sizeof(*rec));
    rec->magic = WATER_RECORD_MAGIC;
    rec->valid_flags = entry->valid_flags;
    rec->timestamp_unix = entry->timestamp_unix;
    rec->ph = entry->ph;
    rec->ammonia_ppm = entry->ammonia_ppm;
    rec->nitrite_ppm = entry->nitrite_ppm;
    rec->nitrate_ppm = entry->nitrate_ppm;
    memcpy(rec->notes, entry->notes, WATER_TEST_NOTES_LEN);
    rec->notes[WATER_TEST_NOTES_LEN - 1] = '\0';
    rec->record_crc = record_crc(rec);
}

static bool record_valid_at(uint16_t index, water_record_t *rec_out)
{
    water_record_t rec;
    if (esp_partition_read(s_part, record_offset(index), &rec, sizeof(rec)) != ESP_OK) {
        return false;
    }
    if (rec.magic != WATER_RECORD_MAGIC) {
        return false;
    }
    if (rec.record_crc != record_crc(&rec)) {
        return false;
    }
    if (rec_out != NULL) {
        *rec_out = rec;
    }
    return true;
}

static esp_err_t rebuild_header_from_scan(water_header_t *hdr)
{
    uint16_t next_write = 0;
    uint16_t count = 0;
    uint16_t first = 0;
    bool found_any = false;

    for (uint16_t i = 0; i < WATER_RECORD_CAPACITY; i++) {
        if (record_valid_at(i, NULL)) {
            if (!found_any) {
                first = i;
                found_any = true;
            }
            count++;
        }
    }

    if (!found_any) {
        hdr->magic = WATER_HEADER_MAGIC;
        hdr->version = WATER_HEADER_VERSION;
        hdr->next_write = 0;
        hdr->count = 0;
        hdr->write_gen = 0;
        hdr->header_crc = header_crc(hdr);
        return write_header(hdr);
    }

    /* Estimate next_write as slot after newest valid record in scan order. */
    uint16_t newest = first;
    for (uint16_t i = 0; i < WATER_RECORD_CAPACITY; i++) {
        uint16_t idx = (uint16_t)((first + i) % WATER_RECORD_CAPACITY);
        if (record_valid_at(idx, NULL)) {
            newest = idx;
        }
    }
    next_write = (uint16_t)((newest + 1U) % WATER_RECORD_CAPACITY);
    if (count > WATER_RECORD_CAPACITY) {
        count = WATER_RECORD_CAPACITY;
    }

    hdr->magic = WATER_HEADER_MAGIC;
    hdr->version = WATER_HEADER_VERSION;
    hdr->next_write = next_write;
    hdr->count = count;
    hdr->write_gen = 0;
    hdr->header_crc = header_crc(hdr);
    ESP_LOGW(TAG, "Rebuilt header: count=%u next_write=%u", (unsigned)count, (unsigned)next_write);
    return write_header(hdr);
}

static uint16_t oldest_index(const water_header_t *hdr)
{
    if (hdr->count == 0) {
        return 0;
    }
    if (hdr->count < WATER_RECORD_CAPACITY) {
        return 0;
    }
    return hdr->next_write;
}

static uint16_t index_for_entry(const water_header_t *hdr, size_t ordinal)
{
    return (uint16_t)((oldest_index(hdr) + ordinal) % WATER_RECORD_CAPACITY);
}

esp_err_t water_storage_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, WATERLOG_PARTITION_SUBTYPE,
                                      WATERLOG_PARTITION_LABEL);
    if (s_part == NULL) {
        ESP_LOGE(TAG, "waterlog partition not found");
        return ESP_ERR_NOT_FOUND;
    }

    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    water_header_t hdr;
    if (read_header(&hdr) != ESP_OK) {
        return ESP_FAIL;
    }

    bool header_ok = (hdr.magic == WATER_HEADER_MAGIC && hdr.version == WATER_HEADER_VERSION &&
                      hdr.header_crc == header_crc(&hdr) && hdr.next_write < WATER_RECORD_CAPACITY &&
                      hdr.count <= WATER_RECORD_CAPACITY);

    if (!header_ok) {
        ESP_LOGW(TAG, "Invalid header; rebuilding");
        if (rebuild_header_from_scan(&hdr) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    s_inited = true;
    ESP_LOGI(TAG, "Water log ready: count=%u capacity=%u", (unsigned)hdr.count, WATER_RECORD_CAPACITY);
    return ESP_OK;
}

esp_err_t water_storage_append(const water_test_entry_t *entry)
{
    if (!s_inited || entry == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    water_header_t hdr;
    esp_err_t err = read_header(&hdr);
    if (err != ESP_OK || hdr.magic != WATER_HEADER_MAGIC) {
        xSemaphoreGive(s_mutex);
        return ESP_FAIL;
    }

    water_record_t rec;
    entry_to_record(entry, &rec);

    err = esp_partition_write(s_part, record_offset(hdr.next_write), &rec, sizeof(rec));
    if (err != ESP_OK) {
        xSemaphoreGive(s_mutex);
        return err;
    }

    hdr.next_write = (uint16_t)((hdr.next_write + 1U) % WATER_RECORD_CAPACITY);
    if (hdr.count < WATER_RECORD_CAPACITY) {
        hdr.count++;
    }
    hdr.write_gen++;
    hdr.header_crc = header_crc(&hdr);
    err = write_header(&hdr);

    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t water_storage_get_latest(water_test_entry_t *out)
{
    if (!s_inited || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    water_header_t hdr;
    if (read_header(&hdr) != ESP_OK || hdr.count == 0) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t idx = index_for_entry(&hdr, hdr.count - 1);
    water_record_t rec;
    if (!record_valid_at(idx, &rec)) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    record_to_entry(&rec, out);
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

size_t water_storage_count(void)
{
    if (!s_inited) {
        return 0;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return 0;
    }

    water_header_t hdr;
    if (read_header(&hdr) != ESP_OK || hdr.magic != WATER_HEADER_MAGIC) {
        xSemaphoreGive(s_mutex);
        return 0;
    }

    size_t count = hdr.count;
    xSemaphoreGive(s_mutex);
    return count;
}

esp_err_t water_storage_get_entries(water_test_entry_t *out, size_t max_entries, size_t *out_count)
{
    if (!s_inited || out == NULL || out_count == NULL || max_entries == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    water_header_t hdr;
    if (read_header(&hdr) != ESP_OK || hdr.count == 0) {
        *out_count = 0;
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }

    size_t n = hdr.count;
    if (n > max_entries) {
        n = max_entries;
    }

    size_t start = hdr.count - n;
    size_t written = 0;
    for (size_t i = 0; i < n; i++) {
        uint16_t idx = index_for_entry(&hdr, start + i);
        water_record_t rec;
        if (!record_valid_at(idx, &rec)) {
            continue;
        }
        record_to_entry(&rec, &out[written]);
        written++;
    }

    *out_count = written;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t water_storage_clear_all(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    water_header_t hdr = {
        .magic = WATER_HEADER_MAGIC,
        .version = WATER_HEADER_VERSION,
        .next_write = 0,
        .count = 0,
        .write_gen = 0,
    };
    hdr.header_crc = header_crc(&hdr);
    esp_err_t err = write_header(&hdr);

    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Water log cleared");
    return err;
}
