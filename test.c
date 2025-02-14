#include <stdio.h>
#include <stdbool.h>

// Định nghĩa kiểu con trỏ hàm, trỏ đến hàm trả về int và không có tham số
typedef int (*func_ptr)();

// Hàm dễ gây lỗi (sử dụng hàm gets)
int vulnerableFunction() {
    char buffer[8];
    // Dùng gets() là lỗi bảo mật kinh điển do không giới hạn độ dài input
    gets(buffer);
    return 0;
}

int main() {
    // Tạo một con trỏ hàm trỏ đến hàm dễ lỗi
    func_ptr fp = vulnerableFunction;

    // NHƯNG KHÔNG BAO GIỜ GỌI fp! (vulnerableFunction không được thực thi)
    // => Valgrind/ASan sẽ không thấy lỗi runtime
    // => GCC có thể chỉ cảnh báo "gets is unsafe" (deprecation) 
    //    chứ không báo tràn bộ đệm, vì nó không 'thấy' việc gọi thực tế.
    
    printf("Hello, nothing to see here...\n");
    return 0;
}
