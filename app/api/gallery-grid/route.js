import { S3Client, ListObjectsV2Command } from '@aws-sdk/client-s3';
import { html } from '@/lib/response';

export const runtime = 'nodejs';
export const dynamic = 'force-dynamic';

export async function GET() {
  const { R2_ENDPOINT, R2_ACCESS_KEY, R2_SECRET_KEY, R2_BUCKET, R2_PUBLIC_URL } = process.env;

  if (!R2_ACCESS_KEY || !R2_SECRET_KEY || !R2_ENDPOINT || !R2_BUCKET) {
    return html('<div class="gallery-error"><i class="fas fa-images"></i><p>No images found.</p></div>');
  }

  try {
    const client = new S3Client({
      region: 'auto',
      endpoint: `https://${R2_ENDPOINT}`,
      credentials: { accessKeyId: R2_ACCESS_KEY, secretAccessKey: R2_SECRET_KEY },
    });

    const { Contents = [] } = await client.send(new ListObjectsV2Command({ Bucket: R2_BUCKET }));
    const baseUrl = R2_PUBLIC_URL || `${R2_BUCKET}.${R2_ENDPOINT}`;
    
    const images = Contents
      .filter(({ Key }) => /\.(jpe?g|png|webp)$/i.test(Key))
      .map(({ Key }) => ({ key: Key, url: `https://${baseUrl}/${Key}` }))
      .sort((a, b) => a.key.localeCompare(b.key));

    const grid = images.map(({ key, url }) => `
      <div class="gallery-item" onclick="openModal('${url}')">
        <img src="${url}" alt="${key}" loading="lazy" decoding="async"/>
        <div class="gallery-item-overlay">${key}</div>
      </div>`).join('');

    return html(`
      <div class="gallery-stats"><i class="fas fa-images"></i> ${images.length} image${images.length !== 1 ? 's' : ''} found</div>
      <div class="gallery-grid">${grid}</div>
    `);
  } catch (e) {
    console.error('Gallery error:', e);
    return html('<div class="gallery-error"><i class="fas fa-exclamation-triangle"></i><p>Failed to load gallery.</p></div>');
  }
}
