// Android AArch64 code, built with the NDK, never compiled into a Windows PE.
// Varargs and va_list stay in the guest domain; only fixed errno/byte/log calls reach the host.
#include <android/log.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

_Static_assert(sizeof(long) == 8 && sizeof(size_t) == 8, "guest LP64, not Windows LLP64");
enum { kFieldLimit = 4096, kResultLimit = 1024 * 1024 };

/// Keeps a bounded snprintf destination separate from its full would-have-written count.
struct output {
    char* bytes;
    size_t capacity, length;
    int error;
};

/// Records one restricted conversion without retaining any host object or allocation.
struct spec {
    int left, plus, space, zero, alternate, width, precision, length;
    char conversion;
};

/// Measures at most limit bytes, allowing an unterminated precision-bounded string.
static size_t text_length(const char* text, size_t limit) {
    size_t length = 0;
    while (length < limit && text[length]) length++;
    return length;
}

/// Appends bytes with truncation while retaining the full result count and an overflow refusal.
static void emit(struct output* output, const char* bytes, size_t size) {
    if (output->error) return;
    if (size > kResultLimit - output->length) { output->error = EOVERFLOW; return; }
    for (size_t i = 0; i < size; i++) {
        if (output->capacity && output->length < output->capacity - 1)
            output->bytes[output->length] = bytes[i];
        output->length++;
    }
}

/// Emits bounded padding without allocating a temporary buffer proportional to its width.
static void padding(struct output* output, char byte, int count) {
    for (int i = 0; i < count; i++) emit(output, &byte, 1);
}

/// Parses a decimal field width/precision and refuses values outside this profile.
static int field(const char** cursor, int* error) {
    int value = 0;
    while (**cursor >= '0' && **cursor <= '9') {
        value = value * 10 + (*(*cursor)++ - '0');
        if (value > kFieldLimit) { *error = EOVERFLOW; return 0; }
    }
    return value;
}

/// Parses one supported conversion using only guest va_arg for integer star arguments.
static struct spec specification(const char** cursor, va_list* arguments, int* error) {
    struct spec spec = {.precision = -1};
    for (;;) {
        switch (**cursor) {
        case '-': spec.left = 1; break;
        case '+': spec.plus = 1; break;
        case ' ': spec.space = 1; break;
        case '0': spec.zero = 1; break;
        case '#': spec.alternate = 1; break;
        default: goto width;
        }
        (*cursor)++;
    }
width:
    if (**cursor == '*') {
        (*cursor)++;
        int64_t value = va_arg(*arguments, int);
        if (value < 0) { spec.left = 1; value = -value; }
        if (value > kFieldLimit) *error = EOVERFLOW;
        else spec.width = (int)value;
    } else {
        spec.width = field(cursor, error);
    }
    if (*error) return spec;
    if (**cursor == '.') {
        (*cursor)++;
        if (**cursor == '*') {
            (*cursor)++;
            const int value = va_arg(*arguments, int);
            spec.precision = value < 0 ? -1 : value;
            if (value > kFieldLimit) *error = EOVERFLOW;
        } else {
            spec.precision = field(cursor, error);
        }
    }
    if (*error) return spec;
    if (**cursor == 'h') {
        (*cursor)++;
        spec.length = 2;
        if (**cursor == 'h') { spec.length = 1; (*cursor)++; }
    } else if (**cursor == 'l') {
        (*cursor)++;
        spec.length = 3;
        if (**cursor == 'l') { spec.length = 4; (*cursor)++; }
    } else if (**cursor == 'z') {
        (*cursor)++;
        spec.length = 5;
    }
    spec.conversion = **cursor;
    if (**cursor) (*cursor)++;
    return spec;
}

/// Renders unsigned magnitude, sign/prefix and integer precision without a host formatter.
static void number(struct output* output, struct spec spec, uint64_t value, int negative, int is_signed) {
    const int pointer = spec.conversion == 'p';
    const int base = spec.conversion == 'x' || spec.conversion == 'X' || pointer ? 16 : 10;
    const char* digits = spec.conversion == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
    char reversed[32], rendered[32], prefix[3];
    int count = 0, prefix_count = 0;
    const uint64_t original = value;
    if (value || spec.precision != 0 || pointer)
        do { reversed[count++] = digits[value % base]; value /= base; } while (value);
    for (int i = 0; i < count; i++) rendered[i] = reversed[count - i - 1];
    if (negative) prefix[prefix_count++] = '-';
    else if (is_signed && spec.plus) prefix[prefix_count++] = '+';
    else if (is_signed && spec.space) prefix[prefix_count++] = ' ';
    if (pointer || (base == 16 && spec.alternate && original)) {
        prefix[prefix_count++] = '0';
        prefix[prefix_count++] = spec.conversion == 'X' ? 'X' : 'x';
    }
    int zeroes = spec.precision > count ? spec.precision - count : 0;
    int spaces = spec.width - prefix_count - count - zeroes;
    if (spaces < 0) spaces = 0;
    if (spec.zero && !spec.left && spec.precision < 0) { zeroes += spaces; spaces = 0; }
    if (!spec.left) padding(output, ' ', spaces);
    emit(output, prefix, (size_t)prefix_count);
    padding(output, '0', zeroes);
    emit(output, rendered, (size_t)count);
    if (spec.left) padding(output, ' ', spaces);
}

/// Formats the declared integer/byte-string subset, refusing unknown conversions before reading them.
static void convert(struct output* output, struct spec spec, va_list* arguments) {
    if (spec.conversion == 'd' || spec.conversion == 'i') {
        int64_t value;
        switch (spec.length) {
        case 1: value = (int8_t)va_arg(*arguments, int); break;
        case 2: value = (int16_t)va_arg(*arguments, int); break;
        case 3: value = va_arg(*arguments, long); break;
        case 4: value = va_arg(*arguments, long long); break;
        case 5: value = va_arg(*arguments, ptrdiff_t); break;
        default: value = va_arg(*arguments, int);
        }
        number(output, spec, value < 0 ? 0 - (uint64_t)value : (uint64_t)value, value < 0, 1);
    } else if (spec.conversion == 'u' || spec.conversion == 'x' || spec.conversion == 'X') {
        uint64_t value;
        switch (spec.length) {
        case 1: value = (uint8_t)va_arg(*arguments, int); break;
        case 2: value = (uint16_t)va_arg(*arguments, int); break;
        case 3: value = va_arg(*arguments, unsigned long); break;
        case 4: value = va_arg(*arguments, unsigned long long); break;
        case 5: value = va_arg(*arguments, size_t); break;
        default: value = va_arg(*arguments, unsigned int);
        }
        number(output, spec, value, 0, 0);
    } else if (spec.length) {
        output->error = ENOTSUP;
    } else if (spec.conversion == 'p') {
        number(output, spec, (uintptr_t)va_arg(*arguments, void*), 0, 0);
    } else if (spec.conversion == 's' || spec.conversion == 'c') {
        char byte = 0;
        const char* text;
        size_t length;
        if (spec.conversion == 'c') {
            byte = (char)va_arg(*arguments, int);
            text = &byte;
            length = 1;
        } else {
            text = va_arg(*arguments, const char*);
            if (!text) text = "(null)";
            length = text_length(text, spec.precision < 0 ? kResultLimit + 1 : (size_t)spec.precision);
            if (length > kResultLimit) { output->error = EOVERFLOW; return; }
        }
        const int spaces = spec.width > (int)length ? spec.width - (int)length : 0;
        if (!spec.left) padding(output, ' ', spaces);
        emit(output, text, length);
        if (spec.left) padding(output, ' ', spaces);
    } else {
        output->error = ENOTSUP;
    }
}

/// Formats guest varargs into bytes with snprintf count/truncation semantics and explicit limits.
int vsnprintf(char* destination, size_t capacity, const char* format, va_list input) {
    if (!format || (capacity && !destination)) { errno = EINVAL; return -1; }
    if (capacity) destination[0] = 0;
    if (text_length(format, kFieldLimit + 1) > kFieldLimit) { errno = EOVERFLOW; return -1; }
    struct output output = {.bytes = destination, .capacity = capacity};
    va_list arguments;
    va_copy(arguments, input);
    while (*format && !output.error) {
        if (*format != '%') { emit(&output, format++, 1); continue; }
        format++;
        if (*format == '%') { emit(&output, format++, 1); continue; }
        struct spec spec = specification(&format, &arguments, &output.error);
        if (!output.error) convert(&output, spec, &arguments);
    }
    va_end(arguments);
    if (output.error) {
        if (capacity) destination[0] = 0;
        errno = output.error;
        return -1;
    }
    if (capacity) destination[output.length < capacity ? output.length : capacity - 1] = 0;
    return (int)output.length;
}

/// Starts and ends an Android va_list entirely in guest code before calling the guest V variant.
int snprintf(char* destination, size_t capacity, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = vsnprintf(destination, capacity, format, arguments);
    va_end(arguments);
    return result;
}

/// Renders a bounded guest log before crossing the fixed-signature byte/log import.
int __android_log_vprint(int priority, const char* tag, const char* format, va_list arguments) {
    char message[4096];
    const int size = vsnprintf(message, sizeof message, format, arguments);
    if (size < 0) return -1;
    if ((size_t)size >= sizeof message) { errno = EOVERFLOW; return -1; }
    return __android_log_write(priority, tag, message);
}

/// Handles Android log varargs in guest code, never in a host va_list or host printf.
int __android_log_print(int priority, const char* tag, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = __android_log_vprint(priority, tag, format, arguments);
    va_end(arguments);
    return result;
}
