export const config = {
  runtime: 'edge',
};

function formatRelativeTime(isoTime) {
  const pushedTime = new Date(isoTime).getTime();
  const now = Date.now();
  const diff = Math.floor((now - pushedTime) / 1000);

  if (diff < 60) {
    return 'just now';
  } else if (diff < 3600) {
    const mins = Math.floor(diff / 60);
    return `${mins}m ago`;
  } else if (diff < 86400) {
    const hours = Math.floor(diff / 3600);
    return `${hours}h ago`;
  } else if (diff < 604800) {
    const days = Math.floor(diff / 86400);
    return `${days}d ago`;
  } else if (diff < 2592000) {
    const weeks = Math.floor(diff / 604800);
    return `${weeks}w ago`;
  } else if (diff < 31536000) {
    const months = Math.floor(diff / 2592000);
    return `${months}mo ago`;
  } else {
    const years = Math.floor(diff / 31536000);
    return `${years}y ago`;
  }
}

export default async function handler(request) {
  const githubRepo = process.env.GITHUB_REPO || '0xjah/0xjah.xyz';

  try {
    const response = await fetch(`https://api.github.com/repos/${githubRepo}`, {
      headers: {
        'User-Agent': '0xjah.xyz/1.0',
      },
    });

    let timeStr = 'recently';

    if (response.ok) {
      const data = await response.json();
      if (data.pushed_at) {
        timeStr = formatRelativeTime(data.pushed_at);
      }
    }

    const html = `<div class="status-header"><h2>Status</h2><p class="quote">Last updated: ${timeStr}</p></div>`;

    return new Response(html, {
      status: 200,
      headers: {
        'Content-Type': 'text/html',
        'Access-Control-Allow-Origin': '*',
      },
    });
  } catch (error) {
    console.error('GitHub status error:', error);
    const html = `<div><h2>Status</h2><p>Last updated: unknown</p></div>`;
    return new Response(html, {
      status: 200,
      headers: {
        'Content-Type': 'text/html',
        'Access-Control-Allow-Origin': '*',
      },
    });
  }
}
