import { serveFile, html } from '@/lib/response';
import { readFileSync, existsSync } from 'fs';
import { join } from 'path';

export const runtime = 'nodejs';

const PUBLIC = join(process.cwd(), 'public');

export async function GET(request) {
  const post = new URL(request.url).searchParams.get('post');
  const isHtmx = request.headers.get('HX-Request') === 'true';
  
  // No post param - serve main blog page
  if (!post) return serveFile('blog.html') || html('Not found', 404);
  
  // Sanitize and get partial
  const safe = post.replace(/[^a-zA-Z0-9_-]/g, '');
  const partialPath = join(PUBLIC, 'partials', 'blog', `${safe}.html`);
  
  if (!existsSync(partialPath)) return html('Post not found', 404);
  
  const partial = readFileSync(partialPath, 'utf-8');
  
  // HTMX request - return just the partial
  if (isHtmx) return html(partial);
  
  // Direct browser request - return full page with partial embedded
  const blogPath = join(PUBLIC, 'blog.html');
  if (!existsSync(blogPath)) return html('Blog not found', 404);
  
  const blogPage = readFileSync(blogPath, 'utf-8');
  const fullPage = blogPage.replace(
    '<div id="blog-post-content" style="margin-top: 20px"></div>',
    `<div id="blog-post-content" style="margin-top: 20px">${partial}</div>`
  );
  
  return html(fullPage);
}
