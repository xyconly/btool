#pragma once
#include <map>
#include <unordered_map>
#include <vector>
#include <string_view>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <arrow/record_batch.h>
#include <arrow/array.h>
#include <parquet/stream_writer.h>
#include <parquet/exception.h>
#include <arrow/io/file.h>

namespace BTool {
    class ParquetRow {
    private:
        struct CloumnInfo {
            std::string         name_;
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

        inline static std::unordered_map<arrow::Type::type, parquet::Type::type> MappingType = {
            {arrow::Type::BOOL, parquet::Type::BOOLEAN},
            {arrow::Type::UINT8, parquet::Type::INT32},
            {arrow::Type::INT8, parquet::Type::INT32},
            {arrow::Type::UINT16, parquet::Type::INT32},
            {arrow::Type::INT16, parquet::Type::INT32},
            {arrow::Type::UINT32, parquet::Type::INT32},
            {arrow::Type::INT32, parquet::Type::INT32},
            {arrow::Type::UINT64, parquet::Type::INT64},
            {arrow::Type::INT64, parquet::Type::INT64},
            {arrow::Type::HALF_FLOAT, parquet::Type::FLOAT},
            {arrow::Type::FLOAT, parquet::Type::FLOAT},
            {arrow::Type::DOUBLE, parquet::Type::DOUBLE},
            {arrow::Type::STRING, parquet::Type::BYTE_ARRAY},
            {arrow::Type::BINARY, parquet::Type::BYTE_ARRAY},
            {arrow::Type::FIXED_SIZE_BINARY, parquet::Type::FIXED_LEN_BYTE_ARRAY},
            {arrow::Type::DATE32, parquet::Type::INT32},
            {arrow::Type::DATE64, parquet::Type::INT64},
            {arrow::Type::TIMESTAMP, parquet::Type::INT64},
            {arrow::Type::TIME32, parquet::Type::INT32},
            {arrow::Type::TIME64, parquet::Type::INT64},
            {arrow::Type::INTERVAL_MONTHS, parquet::Type::INT32},
            {arrow::Type::INTERVAL_DAY_TIME, parquet::Type::INT32},
            {arrow::Type::DECIMAL128, parquet::Type::UNDEFINED},
            {arrow::Type::DECIMAL256, parquet::Type::UNDEFINED},
            {arrow::Type::LIST, parquet::Type::UNDEFINED},
            {arrow::Type::STRUCT, parquet::Type::UNDEFINED},
            {arrow::Type::SPARSE_UNION, parquet::Type::UNDEFINED},
            {arrow::Type::DENSE_UNION, parquet::Type::UNDEFINED},
            {arrow::Type::DICTIONARY, parquet::Type::UNDEFINED},
            {arrow::Type::MAP, parquet::Type::UNDEFINED},
            {arrow::Type::EXTENSION, parquet::Type::UNDEFINED},
            {arrow::Type::FIXED_SIZE_LIST, parquet::Type::UNDEFINED},
            {arrow::Type::DURATION, parquet::Type::UNDEFINED},
            {arrow::Type::LARGE_STRING, parquet::Type::UNDEFINED},
            {arrow::Type::LARGE_BINARY, parquet::Type::UNDEFINED},
            {arrow::Type::LARGE_LIST, parquet::Type::UNDEFINED},
            {arrow::Type::INTERVAL_MONTH_DAY_NANO, parquet::Type::UNDEFINED},
            {arrow::Type::RUN_END_ENCODED, parquet::Type::UNDEFINED},
        };
    
        inline static std::unordered_map<arrow::Type::type, parquet::ConvertedType::type> ConvertedMappingType = {
            {arrow::Type::BOOL, parquet::ConvertedType::UINT_8},
            {arrow::Type::UINT8, parquet::ConvertedType::UINT_8},
            {arrow::Type::INT8, parquet::ConvertedType::INT_8},
            {arrow::Type::UINT16, parquet::ConvertedType::UINT_16},
            {arrow::Type::INT16, parquet::ConvertedType::INT_16},
            {arrow::Type::UINT32, parquet::ConvertedType::UINT_32},
            {arrow::Type::INT32, parquet::ConvertedType::INT_32},
            {arrow::Type::UINT64, parquet::ConvertedType::UINT_64},
            {arrow::Type::INT64, parquet::ConvertedType::INT_64},
            {arrow::Type::HALF_FLOAT, parquet::ConvertedType::DECIMAL},
            {arrow::Type::FLOAT, parquet::ConvertedType::DECIMAL},
            {arrow::Type::DOUBLE, parquet::ConvertedType::DECIMAL},
            {arrow::Type::STRING, parquet::ConvertedType::UTF8},
            {arrow::Type::BINARY, parquet::ConvertedType::NONE},
            {arrow::Type::FIXED_SIZE_BINARY, parquet::ConvertedType::NONE},
            {arrow::Type::DATE32, parquet::ConvertedType::DATE},
            {arrow::Type::DATE64, parquet::ConvertedType::DATE},
            {arrow::Type::TIMESTAMP, parquet::ConvertedType::TIME_MILLIS},
            {arrow::Type::TIME32, parquet::ConvertedType::TIME_MILLIS},
            {arrow::Type::TIME64, parquet::ConvertedType::TIME_MICROS},
            {arrow::Type::INTERVAL_MONTHS, parquet::ConvertedType::INTERVAL},
            {arrow::Type::INTERVAL_DAY_TIME, parquet::ConvertedType::INTERVAL},
            {arrow::Type::DECIMAL128, parquet::ConvertedType::DECIMAL},
            {arrow::Type::DECIMAL256, parquet::ConvertedType::DECIMAL},
            {arrow::Type::LIST, parquet::ConvertedType::LIST},
            {arrow::Type::STRUCT, parquet::ConvertedType::NONE},
            {arrow::Type::SPARSE_UNION, parquet::ConvertedType::NONE},
            {arrow::Type::DENSE_UNION, parquet::ConvertedType::NONE},
            {arrow::Type::DICTIONARY, parquet::ConvertedType::NONE},
            {arrow::Type::MAP, parquet::ConvertedType::MAP},
            {arrow::Type::EXTENSION, parquet::ConvertedType::NONE},
            {arrow::Type::FIXED_SIZE_LIST, parquet::ConvertedType::LIST},
            {arrow::Type::DURATION, parquet::ConvertedType::NONE},
            {arrow::Type::LARGE_STRING, parquet::ConvertedType::UTF8},
            {arrow::Type::LARGE_BINARY, parquet::ConvertedType::NONE},
            {arrow::Type::LARGE_LIST, parquet::ConvertedType::LIST},
            {arrow::Type::INTERVAL_MONTH_DAY_NANO, parquet::ConvertedType::INTERVAL},
            {arrow::Type::RUN_END_ENCODED, parquet::ConvertedType::NONE},
        };
        
    public:
        class Reader {
        public:
            Reader() = default;
            ~Reader() {
                close();
            }

            void close() {
                m_reader.reset();
                m_arrow_reader.reset();
                m_cloumn_infos.clear();
                m_fix_cloumn_sizes.clear();

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

                m_cloumn_infos.reserve(schema->num_fields());

                for (auto& item : schema->fields()) {
                    int cloumn_length = get_column_fix_length(item);
                    if (cloumn_length == 0) // 历史原因, 不知道谁存储的时候居然不存储fixstring
                        return false;
                    m_cloumn_infos.emplace_back(CloumnInfo{item->name(), cloumn_length, m_row_length, item->type()->id()});
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

            inline size_t get_row_count() const {
                return m_num_rows;
            }

            inline int get_row_length() const {
                return m_row_length;
            }

            inline int get_column_length(int column) const {
                return m_cloumn_infos[column].fix_length_;
            }

            inline arrow::Type::type get_column_type(int column) const {
                return m_cloumn_infos[column].data_type_;
            }

            inline int get_column_count() const {
                return m_num_columns;
            }

            inline const std::string& get_column_name(int column) const {
                return m_cloumn_infos[column].name_;
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
                    char* buf = m_read_buf + m_cloumn_infos[column_index++].row_offset_;
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
                //         row_offset += m_item_size;
                //     }
                // }
            }

        private:
            // 自定义列长度
            std::map<std::string, int>                  m_fix_cloumn_sizes;
            // 列信息
            std::vector<CloumnInfo>                     m_cloumn_infos;
            // 实际文件的单行长度
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

        class Writer {
        public:
            void set_column(size_t index, const std::string& title, int fix_length, arrow::Type::type data_type) {
                if (index >= m_cloumn_infos.capacity()) {
                    m_cloumn_infos.resize(index + 1);
                }
                m_cloumn_infos[index] = CloumnInfo{title, fix_length, 0, data_type};

                // 对之后的做位移
                int cur_off = 0;
                if (index > 0) {
                    auto& pre_item = m_cloumn_infos[index - 1];
                    cur_off = pre_item.row_offset_ + pre_item.fix_length_;
                }
                for(size_t i = index; i < m_cloumn_infos.size(); ++i) {
                    auto& cur_item = m_cloumn_infos[i];
                    cur_item.row_offset_ = cur_off;
                    cur_off += cur_item.fix_length_;
                }
                m_row_length = cur_off;
            }

            bool open(const char* file, int chunk_size = 100000, bool append = true, bool create = true) {
                if (m_cloumn_infos.empty()) {
                    return false;
                }
                for(auto& item : m_cloumn_infos) {
                    if (item.name_.empty() || item.fix_length_ == 0)
                        return false;
                    //m_fields.emplace_back(arrow::field(item.name_, item.data_type_));
                }

                m_chunk_size = chunk_size;
                auto status_name = arrow::io::FileOutputStream::Open(file, append);
                if (!arrow::internal::GenericToStatus(status_name.status()).ok()) {
                    return false;
                }
                m_file = *status_name;

                parquet::WriterProperties::Builder builder;
#if defined ARROW_WITH_BROTLI
                builder.compression(parquet::Compression::BROTLI);
#elif defined ARROW_WITH_ZSTD
                builder.compression(parquet::Compression::ZSTD);
#endif

                m_schema = create_schema();
                m_writer = std::make_shared<parquet::StreamWriter>(parquet::ParquetFileWriter::Open(m_file, m_schema, builder.build()));
                
                m_writer->SetMaxRowGroupSize(chunk_size);
                //m_builder = std::make_shared<arrow::StructBuilder>(m_schema, arrow::default_memory_pool());
            }

            template<typename Type>
            bool write(const Type& data) {
                for (int index = 0; index < m_cloumn_infos.size(); ++index) {
                    auto& field = m_fields[index];
                    auto& column_item = m_cloumn_infos[index];
                    const char* buf = (const char*)(&data) + column_item.row_offset_;
                    switch (column_item.data_type_) {
                    case arrow::Type::BOOL:
                        write_stream<arrow::BooleanType>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::INT8:
                        write_stream<arrow::Int8Type>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::UINT8:
                        write_stream<arrow::UInt8Type>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::INT16:
                        write_stream<arrow::Int16Type>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::UINT16:
                        write_stream<arrow::UInt16Type>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::INT32:
                        write_stream<arrow::Int32Type>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::UINT32:
                        write_stream<arrow::UInt32Type>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::INT64:
                        write_stream<arrow::Int64Type>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::UINT64:
                        write_stream<arrow::UInt64Type>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::HALF_FLOAT:
                        write_stream<arrow::HalfFloatType>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::FLOAT:
                        write_stream<arrow::FloatType>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::DOUBLE:
                        write_stream<arrow::DoubleType>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::STRING:
                        //write_stream_fix_view<arrow::StringType>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::LARGE_STRING:
                        //write_stream_fix_view<arrow::LargeStringType>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::BINARY:
                        //write_stream_fix_view<arrow::BinaryType>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::LARGE_BINARY:
                        //write_stream_fix_view<arrow::LargeBinaryType>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::FIXED_SIZE_BINARY:
                        //write_stream_fix_view<arrow::FixedSizeBinaryType>(buf, column_item.fix_length_);
                        break;
                    case arrow::Type::NA:
                        //write_stream_na_view(buf, column_item.fix_length_);
                        break;
                    default:
                        return false;
                        break;
                    }
                }
                *m_writer << parquet::EndRow;
            }

            template<typename TypeClass>
            bool write_stream(const char* data, int length) {
                if (sizeof(typename TypeClass::c_type) > length)
                    return false;
                *m_writer << *(const typename TypeClass::c_type *)(data);
            }

            // template<typename TypeClass>
            // bool write_stream_fix_view(const char* data, int length) {
            //     if (sizeof(TypeClass::c_type) > length)
            //         return false;
            //     m_writer << *(const TypeClass::c_type*)(data);
            // }

            // template<typename TypeClass>
            // bool write_stream_na_view(const char* data, int length) {
            //     if (sizeof(TypeClass::c_type) > length)
            //         return false;
            //     m_writer << *(const TypeClass::c_type*)(data);
            // }

    //         template<typename Type>
    //         bool write(const Type& data) {
    //             // 创建 Arrow 结构体构建器
    //             arrow::StructBuilder struct_builder();
    //             // 填充 Arrow 数据
    //             for (int i = 0; i < 10; i++) {
    //                 Type data;
    //                 data.field1 = i;
    //                 data.field2 = static_cast<double>(i) + 0.5;
    //                 // 使用结构体的数据填充 Arrow 数据
    //                 struct_builder.Append();
    //                 for(int index = 0; index < m_cloumn_infos.size(); ++index) {
    //                     m_cloumn_infos[index]
    //                     struct_builder.
    //   std::shared_ptr<arrow::ArrayBuilder> child_builder = builder->child_builder(i);->Append(data.field2);
    //                 }
    //                  [0]->Append(data.field1);
    //                 struct_builder[1]->Append(data.field2);
    //             }
    //         }
            // bool write(const char* data, size_t len) {
            //     if (len < m_row_length) return false;
            //     if (m_cloumn_infos.empty()) return true;
            //     for(int index = 0; index < m_cloumn_infos.size(); ++index) {
            //         auto& column_item = m_cloumn_infos[index];
            //         auto& builder_base_ptr = m_cloumn_values[index];
            //         switch (column_item.data_type_) {
            //         case arrow::Type::type::BOOL:
            //             build_append<BooleanBuilder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::UINT8:
            //             build_append<UInt8Builder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::INT8:
            //             build_append<Int8Builder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::UINT16:
            //             build_append<UInt16Builder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::INT16:
            //             build_append<Int16Builder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::UINT32:
            //             build_append<UInt32Builder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::INT32:
            //             build_append<Int32Builder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::UINT64:
            //             build_append<UInt64Builder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::INT64:
            //             build_append<Int64Builder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::HALF_FLOAT:
            //             build_append<HalfFloatBuilder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::FLOAT:
            //             build_append<FloatBuilder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::DOUBLE:
            //             build_append<DoubleBuilder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::STRING:
            //             build_append<DoubleBuilder>(builder_base_ptr, column_item, data);
            //             break;
            //         case arrow::Type::type::BINARY:
            //             break;
            //         case arrow::Type::type::FIXED_SIZE_BINARY:
            //             break;
            //         case arrow::Type::type::DATE32:
            //             break;
            //         case arrow::Type::type::DATE64:
            //             break;
            //         case arrow::Type::type::TIMESTAMP:
            //             break;
            //         case arrow::Type::type::TIME32:
            //             break;
            //         case arrow::Type::type::TIME64:
            //             break;
            //         case arrow::Type::type::INTERVAL_MONTHS:
            //             break;
            //         case arrow::Type::type::INTERVAL_DAY_TIME:
            //             break;
            //         case arrow::Type::type::DECIMAL128:
            //             break;
            //         case arrow::Type::type::DECIMAL:
            //             break;
            //         case arrow::Type::type::DECIMAL256:
            //             break;
            //         case arrow::Type::type::LIST:
            //             break;
            //         case arrow::Type::type::STRUCT:
            //             break;
            //         case arrow::Type::type::SPARSE_UNION:
            //             break;
            //         case arrow::Type::type::DENSE_UNION:
            //             break;
            //         case arrow::Type::type::DICTIONARY:
            //             break;
            //         case arrow::Type::type::MAP:
            //             break;
            //         case arrow::Type::type::EXTENSION:
            //             break;
            //         case arrow::Type::type::FIXED_SIZE_LIST:
            //             break;
            //         case arrow::Type::type::DURATION:
            //             break;
            //         case arrow::Type::type::LARGE_STRING:
            //             break;
            //         case arrow::Type::type::LARGE_BINARY:
            //             break;
            //         case arrow::Type::type::LARGE_LIST:
            //             break;
            //         case arrow::Type::type::INTERVAL_MONTH_DAY_NANO:
            //             break;
            //         case arrow::Type::type::RUN_END_ENCODED:
            //             break;
            //         case arrow::Type::type::MAX_ID:
            //             break;
            //         }
            //     }
            // }
        // private:
        //     template<typename T>
        //     void build_append(std::shared_ptr<arrow::ArrayBuilder>& builder_base_ptr, CloumnInfo& column_item, const char* data) {
        //         std::shared_ptr<T> builder_ptr = std::static_pointer_cast<T>(builder_base_ptr);
        //         builder_ptr->Append(*(T::value_type*)(data + column_item.row_offset_));
        //     }

        private:
            std::shared_ptr<parquet::schema::GroupNode> create_schema() {
                parquet::schema::NodeVector fields;
                for(auto& item : m_cloumn_infos) {
                    fields.push_back(parquet::schema::PrimitiveNode::Make(item.name_, parquet::Repetition::REQUIRED
                                        , MappingType[item.data_type_], ConvertedMappingType[item.data_type_]));
                }

                return std::static_pointer_cast<parquet::schema::GroupNode>(
                    parquet::schema::GroupNode::Make("schema", parquet::Repetition::REQUIRED, fields));
            }

        private:
            std::shared_ptr<arrow::io::FileOutputStream>        m_file = nullptr;
            std::shared_ptr<parquet::StreamWriter>              m_writer = nullptr;

            int                                                 m_chunk_size = 0;
            // 单行长度
            size_t                                              m_row_length = 0;
            // 列信息
            std::vector<CloumnInfo>                             m_cloumn_infos;
            //std::shared_ptr<arrow::StructBuilder>               m_builder;
            //std::shared_ptr<arrow::WriterProperties>            m_builder;

            arrow::FieldVector                                  m_fields;
            std::shared_ptr<parquet::schema::GroupNode>         m_schema = nullptr;
        };
    };
}