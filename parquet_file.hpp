#pragma once
#include <map>
#include <vector>
#include <string_view>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <arrow/record_batch.h>
#include <arrow/array.h>

class ParquetRowReader {
    struct CloumnInfo {
        const char*         name_;
        int                 fix_length_;
        int                 row_offset_;
        arrow::Type::type   data_type_;
    };

    enum {
        STRING_FIX_LENGTH       = 40,
        LARGE_STRING_FIX_LENGTH = 512,
        BINARY_FIX_LENGTH       = 1024,
        LARGE_BINARY_FIX_LENGTH = 4096,
        NA_FIX_LENGTH           = 1024,
    };

public:
    ParquetRowReader() = default;
    ~ParquetRowReader() {
        if (m_read_buf_len > 0 && m_read_buf) {
            free(m_read_buf);
            m_read_buf = nullptr;
        }
    }

    // 设置列长度
    void set_column(const std::string& column_name, int size) {
        m_fix_cloumn_sizes[column_name] = size;
    }

    // 是否多线程读取, 注意多线程读取时会导致内存增多的情况, 不过也在可接受范围内
    // 多线程虽然可以节省一半的内存, 但是大多数情况下性能开销并非在此处, 而是后续数据处理
    bool open(const char* file, int chunk_size = 100000, bool use_threads = false) {
        arrow::MemoryPool* pool = arrow::default_memory_pool();
        // Configure general Parquet reader settings
        auto reader_properties = parquet::ReaderProperties(pool);

        // Configure Arrow-specific Parquet reader settings
        auto arrow_reader_props = parquet::ArrowReaderProperties(use_threads);
        arrow_reader_props.set_batch_size(chunk_size);

        // 启用预加载
        // arrow_reader_props.set_pre_buffer(true);
        // reader_properties.set_buffer_size(4096 * 100);
        // reader_properties.enable_buffered_stream();

        parquet::arrow::FileReaderBuilder reader_builder;
        reader_builder.memory_pool(pool);
        reader_builder.properties(arrow_reader_props);

        auto reader_staus = reader_builder.OpenFile(file, /*memory_map=*/false, reader_properties);
        if (!reader_staus.ok()) {
            return false;
        }

        auto arrow_reader_staus = reader_builder.Build(&m_arrow_reader);
        if (!arrow_reader_staus.ok() || !m_arrow_reader) {
            return false;
        }

        std::shared_ptr<arrow::Schema> schema;
        if (!m_arrow_reader->GetSchema(&schema).ok()) {
            return false;
        }

        m_cloumn_info.reserve(schema->num_fields());

        for (auto& item : schema->fields()) {
            int cloumn_length = get_column_fix_length(item);
            if (cloumn_length == 0) // 历史原因, 不知道谁存储的时候居然不存储fixstring
                return false;
            m_cloumn_info.emplace_back(CloumnInfo{item->name().c_str(), cloumn_length, m_row_length, item->type()->id()});
            m_row_length += cloumn_length;
        }

        m_num_rows = m_arrow_reader->parquet_reader()->metadata()->num_rows();
        m_num_columns = m_arrow_reader->parquet_reader()->metadata()->num_columns();

        auto read_ok = m_arrow_reader->GetRecordBatchReader(&m_reader);
        if (!read_ok.ok()) {
            return false;
        }
        m_read_buf_len = chunk_size * m_row_length;
        m_read_buf = (char* )malloc(m_read_buf_len);
        return true;
    }

    inline int get_row_length() const {
        return m_row_length;
    }

    inline int get_column_length(int column) const {
        return m_cloumn_info[column].fix_length_;
    }

    inline arrow::Type::type get_column_type(int column) const {
        return m_cloumn_info[column].data_type_;
    }

    inline size_t get_row_count() const {
        return m_num_rows;
    }

    inline int get_column_count() const {
        return m_num_columns;
    }

    inline const char* get_column_name(int column) const {
        return m_cloumn_info[column].name_;
    }

    template <typename T>
    std::tuple<bool, const T*, size_t/*len*/> read() {
        auto [ok, content] = read();
        if (!ok)
            return std::forward_as_tuple(ok, nullptr, 0);
        return std::forward_as_tuple(ok, (const T*)content.data(), content.length() / sizeof(T));
    }

    std::tuple<bool, std::string_view> read() {
        memset(m_read_buf, 0, m_read_buf_len);
        auto status = m_reader->Next();
        if (!status.ok()) {
            return std::forward_as_tuple(false, std::string_view{});
        }
        auto& batch = status.ValueOrDie();
        if (!batch) { // 已读取至结尾
            return std::forward_as_tuple(true, std::string_view{});
        }
        auto& column_datas = batch->columns();
        int column_index = 0;
        for (auto& array : column_datas) {
            char* buf = m_read_buf + m_cloumn_info[column_index++].row_offset_;
            switch (array->type_id()) {
            case arrow::Type::BOOL:
                append_stream<arrow::BooleanArray>(array, buf);
                break;
            case arrow::Type::INT8:
                append_stream<arrow::Int8Array>(array, buf);
                break;
            case arrow::Type::UINT8:
                append_stream<arrow::UInt8Array>(array, buf);
                break;
            case arrow::Type::INT16:
                append_stream<arrow::Int16Array>(array, buf);
                break;
            case arrow::Type::UINT16:
                append_stream<arrow::UInt16Array>(array, buf);
                break;
            case arrow::Type::INT32:
                append_stream<arrow::Int32Array>(array, buf);
                break;
            case arrow::Type::UINT32:
                append_stream<arrow::UInt32Array>(array, buf);
                break;
            case arrow::Type::INT64:
                append_stream<arrow::Int64Array>(array, buf);
                break;
            case arrow::Type::UINT64:
                append_stream<arrow::UInt64Array>(array, buf);
                break;
            case arrow::Type::HALF_FLOAT:
                append_stream<arrow::HalfFloatArray>(array, buf);
                break;
            case arrow::Type::FLOAT:
                append_stream<arrow::FloatArray>(array, buf);
                break;
            case arrow::Type::DOUBLE:
                append_stream<arrow::DoubleArray>(array, buf);
                break;
            case arrow::Type::STRING:
                append_stream_fix_view<arrow::StringArray>(array, buf);
                break;
            case arrow::Type::LARGE_STRING:
                append_stream_fix_view<arrow::LargeStringArray>(array, buf);
                break;
            case arrow::Type::BINARY:
                append_stream_fix_view<arrow::BinaryArray>(array, buf);
                break;
            case arrow::Type::LARGE_BINARY:
                append_stream_fix_view<arrow::LargeBinaryArray>(array, buf);
                break;
            case arrow::Type::FIXED_SIZE_BINARY:
                append_stream_fix_view<arrow::FixedSizeBinaryArray>(array, buf);
                break;
            case arrow::Type::NA:
                append_stream_na_view(array, buf);
                break;
            default:
                return std::forward_as_tuple(false, std::string_view{});
                break;
            }
        }

        return std::forward_as_tuple(true, std::string_view(m_read_buf, batch->num_rows() * m_row_length));
    }

private:
    int get_column_fix_length(const std::shared_ptr<arrow::Field>& field) {
        auto width = field->type()->byte_width();
        if (width > 0) {
            return width;
        }
        auto const& column_name = field->type()->name();
        auto iter = m_fix_cloumn_sizes.find(column_name);
        if (iter != m_fix_cloumn_sizes.end()) {
            return iter->second;
        }
        switch (field->type()->id()) {
        // case arrow::Type::BOOL:
        // case arrow::Type::UINT8:
        // case arrow::Type::INT8:
        //     return 1;
        // case arrow::Type::UINT16:
        // case arrow::Type::INT16:
        // case arrow::Type::HALF_FLOAT:
        //     return 2;
        // case arrow::Type::UINT32:
        // case arrow::Type::INT32:
        // case arrow::Type::FLOAT:
        // case arrow::Type::DATE32:
        // case arrow::Type::TIME32:
        //     return 4;
        // case arrow::Type::UINT64:
        // case arrow::Type::INT64:
        // case arrow::Type::DOUBLE:
        // case arrow::Type::DATE64:
        // case arrow::Type::TIMESTAMP:
        // case arrow::Type::TIME64:
        //     return 8;
        // case arrow::Type::DECIMAL128:
        //     return 16;
        // case arrow::Type::DECIMAL256:
        //     return 32;
        case arrow::Type::STRING:
            return STRING_FIX_LENGTH;
        case arrow::Type::LARGE_STRING:
            return LARGE_STRING_FIX_LENGTH;
        case arrow::Type::BINARY:
            return BINARY_FIX_LENGTH;
        case arrow::Type::LARGE_BINARY:
            return LARGE_BINARY_FIX_LENGTH;
        case arrow::Type::NA:
            return NA_FIX_LENGTH;
        default:
            break;
        }
        return 0;
    }

    template <typename T, typename TArr>
    void append_stream(TArr& array, char* read_buf) {
        auto tmp_array = std::static_pointer_cast<T>(array);
        for (auto iter = tmp_array->begin(); iter != tmp_array->end(); ++iter){
            auto const & tmp = **iter;
            memcpy(read_buf, (const void*)&tmp, sizeof(tmp));
            read_buf += m_row_length;
        }
    }

    template <typename T, typename TArr>
    void append_stream_fix_view(TArr& array, char* read_buf) {
        size_t row_offset = 0;
        auto tmp_array = std::static_pointer_cast<T>(array);
        for (int64_t row_index = 0; row_index < array->length(); ++row_index) {
            std::string_view tmp = tmp_array->GetView(row_index);
            if (!tmp.empty()) {
                memcpy(read_buf + row_offset, tmp.data(), tmp.length());
            }
            row_offset += m_row_length;
        }
    }

    template <typename TArr>
    void append_stream_na_view(TArr& array, char* read_buf){
        return;
        // size_t row_offset = 0;
        // auto tmp_array = std::static_pointer_cast<arrow::Nul1Array>(array);
        // const std::shared_ptr<arrow::ArrayData>& data = tmp_array->data();
        // for (int i = 0; i < tmp_array->length(); i++) {
        //     if (tmp_array->IsValid(i)) {
        //         auto ret = data->GetValues<char>(i);
        //         // if (ret != nullptr) {
        //         //     memcpy(read buf + row offset, ret, strlen(ret)) ?
        //         // }
        //         row_offset += m_row_length;
        //     }
        // }
    }

private:
    // 自定义列长度
    std::map<std::string, int>                  m_fix_cloumn_sizes;
    // 列信息
    std::vector<CloumnInfo>                     m_cloumn_info;
    // 单行长度
    int                                         m_row_length = 0;
    // 文件总行数
    size_t                                      m_num_rows = 0;
    // 文件总列数
    int                                         m_num_columns = 0;
    // 读句柄
    std::shared_ptr<arrow::RecordBatchReader>   m_reader = nullptr;
    std::unique_ptr<parquet::arrow::FileReader> m_arrow_reader = nullptr;
    // 读缓存
    char*                                       m_read_buf = nullptr;
    size_t                                      m_read_buf_len = 0;
};