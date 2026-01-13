import { serveFile, html } from '@/lib/response';

export const runtime = 'nodejs';

export async function GET(request, { params }) {
  const path = params.path.join('/');
  
  // Prevent path traversal
  if (path.includes('..')) return html('Invalid path', 400);
  
  return serveFile(`partials/${path}`) || html('Not found', 404);
}
