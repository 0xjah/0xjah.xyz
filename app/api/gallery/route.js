import { S3Client, ListObjectsV2Command } from '@aws-sdk/client-s3';
import { json } from '@/lib/response';

export const runtime = 'nodejs';
export const dynamic = 'force-dynamic';

export async function GET() {
  const { R2_ENDPOINT, R2_ACCESS_KEY, R2_SECRET_KEY, R2_BUCKET, R2_PUBLIC_URL } = process.env;

  if (!R2_ACCESS_KEY || !R2_SECRET_KEY || !R2_ENDPOINT || !R2_BUCKET) {
    return json({ images: [], count: 0 });
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
      .map(({ Key }) => ({ name: Key, key: Key, url: `https://${baseUrl}/${Key}` }))
      .sort((a, b) => a.key.localeCompare(b.key));

    return json({ images, count: images.length });
  } catch (e) {
    console.error('Gallery error:', e);
    return json({ error: 'Failed to fetch from R2', images: [], count: 0 }, 500);
  }
}
