class Image {
public:
    int m_width;
    int m_height;
    int m_quality = 100;

    Image(int width, int height);

    void PrepareTileParts(void* data, int dataLength);
};