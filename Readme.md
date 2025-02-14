# Giới thiệu

Trong bối cảnh các dự án mã nguồn mở ngày càng phức tạp, việc đảm bảo tính bảo mật của mã nguồn là một thách thức lớn. Clang, với khả năng phân tích tĩnh mạnh mẽ, đã trở thành một công cụ không thể thiếu trong việc phát hiện các lỗi bảo mật. Đầu tiên, Clang chuyển đổi mã nguồn thành một cây AST, cho phép chúng ta truy xuất và duyệt qua từng phần của mã một cách chính xác. Điều này giúp xác định các mẫu mã dễ gây lỗi như sử dụng các hàm nguy hiểm (ví dụ: strcpy, sprintf, gets) hoặc các lỗi liên quan đến quản lý bộ nhớ (memory leak, double free, use-after-free).

Thông qua libclang, ta có thể xây dựng các công cụ phân tích tĩnh tùy chỉnh để quét và so sánh các lời gọi hàm trong mã nguồn. Điều này không chỉ giúp phát hiện lỗi ngay cả trong các phần code chưa được chạy (ví dụ, mã “chết” hoặc các tình huống sử dụng gián tiếp thông qua con trỏ hàm), mà còn cho phép tích hợp vào quy trình CI/CD để ngăn ngừa việc đưa vào sản phẩm các lỗ hổng tiềm ẩn.

Ngoài ra, Clang có khả năng cung cấp các thông báo lỗi và cảnh báo cực kỳ chi tiết, chỉ ra chính xác vị trí lỗi trong mã nguồn, điều này giúp các lập trình viên dễ dàng xác định và sửa chữa lỗi bảo mật. Sự mở rộng về mặt API và khả năng tích hợp của Clang đã tạo điều kiện cho việc xây dựng các công cụ phân tích nâng cao, từ đó giúp các dự án mã nguồn mở cải thiện đáng kể chất lượng và an toàn của phần mềm.

# Compile
```
gcc main.c -o mytool \
    -I/usr/lib/llvm-14/include \
    -L/usr/lib/llvm-14/lib \
    -lclang
```
# Test 

```
./mytool test.c
```

# Result
```
mydev@mydev-virtual-machine:~/Optimize/Checker$ ./mytool test.c

=== Analyzing: test.c ===
[WARNING] Potentially unsafe function 'gets' at test.c:11:5
[WARNING] Format string bug with 'printf' at test.c:24:5
```