#include "resume.h"

#include "../core/http.h"
#include "../utils/buffer.h"
#include "static.h"

void resume_serve(int fd) {
    Buffer buf = {0};

    if (static_read_file("public/static/resume.pdf", &buf) < 0) {
        http_error(fd, 404, "Resume not found");
        return;
    }

    http_send(fd,
              200,
              "application/pdf",
              buf.data,
              buf.size,
              "Content-Disposition: attachment; filename=\"ahmad_jahaf_resume.pdf\"\r\n");
    buffer_free(&buf);
}
