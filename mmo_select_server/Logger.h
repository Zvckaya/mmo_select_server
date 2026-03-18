#pragma once

#include <cstddef>
#include <string_view>
#include <filesystem>
#include <cstdio>
#include "CRingBuffer.h"

class Logger
{
public:
	static constexpr std::size_t BUFFER_SIZE     = 65536;  // 내부 링버퍼 크기 (바이트)
	static constexpr std::size_t MAX_LINE_LENGTH = 512;    // 한 줄 최대 길이

	enum class LogLevel   { Debug, Info, Warning, Error, Fatal };
	enum class SinkBackend { STDIO, POSIX, WINDOWS };
	enum class SinkTarget  { STDOUT, STDERR, FILE };

	// 싱글턴 인스턴스 반환
	static Logger& get_instance() noexcept;

	// 로그 기록 (string_view / printf 스타일)
	bool log(LogLevel level, std::string_view str) noexcept;
	bool log(LogLevel level, const char* fmtstr, ...) noexcept;

	// 내부 버퍼를 출력 대상(sink)으로 비우기
	bool flush(void) noexcept;

	// 임계 레벨 조회/설정 (임계 미만 레벨은 기록 건너뜀)
	LogLevel get_threshold(void) const noexcept;
	void     set_threshold(LogLevel level) noexcept;

	// 출력 백엔드 전환 (STDIO / POSIX / WINDOWS)
	bool set_backend(SinkBackend backend) noexcept;

	// 출력 대상 전환
	bool set_target_stdout(void) noexcept;
	bool set_target_stderr(void) noexcept;
	bool set_target_file(const std::filesystem::path& filepath) noexcept;

private:
	// 출력 대상(Sink) 내부 구조체
	struct Sink
	{
		SinkBackend           backend;
		SinkTarget            target;
		std::filesystem::path filepath;
		union {
			FILE* fp;               // STDIO 백엔드
			int   fd;               // POSIX 백엔드
			void* windows_handle;   // WINDOWS 백엔드 (HANDLE)
		} handle;

		Sink(void) noexcept;
		~Sink(void) noexcept;
		Sink(Sink&&) noexcept;
		Sink& operator=(Sink&&) noexcept;

		Sink(const Sink&)            = delete;
		Sink& operator=(const Sink&) = delete;

		void        close(void) noexcept;
		bool        open(void) noexcept;
		std::size_t write(const char* buffer, std::size_t count) const noexcept;
	};

	Logger(void) noexcept;
	~Logger(void) noexcept;
	Logger(const Logger&)            = delete;
	Logger& operator=(const Logger&) = delete;

	bool is_enabled(LogLevel level) const noexcept;

	CRingBuffer buffer;     // 로그 문자열을 임시 저장하는 링버퍼
	LogLevel    threshold;  // 최소 출력 레벨
	Sink        sink;       // 실제 출력 대상
};
