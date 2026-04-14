#include "resume.h"

#include "../core/http.h"
#include "../utils/buffer.h"
#include "static.h"

void resume_serve(int fd) {
    Buffer response_buffer = {0};

    if (static_read_file("web/assets/documents/resume.pdf", &response_buffer) < 0) {
        http_error(fd, 404, "Resume not found");
        return;
    }

    http_send(fd,
              200,
              "application/pdf",
              response_buffer.data,
              response_buffer.size,
              "Content-Disposition: attachment; filename=\"ahmad_jahaf_resume.pdf\"\r\n");
    buffer_free(&response_buffer);
}
