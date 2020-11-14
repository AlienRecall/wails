package logger

// LogLevel is an unsigned 8bit int
type LogLevel uint8

const (
	// TRACE level
	TRACE LogLevel = 1

	// DEBUG level logging
	DEBUG LogLevel = 2

	// INFO level logging
	INFO LogLevel = 3

	// WARNING level logging
	WARNING LogLevel = 4

	// ERROR level logging
	ERROR LogLevel = 5
)

// Logger specifies the methods required to attach
// a logger to a Wails application
type Logger interface {
	Print(message string) error
	Trace(message string) error
	Debug(message string) error
	Info(message string) error
	Warning(message string) error
	Error(message string) error
	Fatal(message string) error
}
