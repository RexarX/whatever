#!/usr/bin/env python3
"""
Download face detection models for the FaceTracker application.

This script downloads pre-trained face detection models in multiple formats:
- ONNX format (YuNet - Modern, single file, recommended)
- Caffe format (MobileNet-SSD - Classic, two files: .caffemodel and .prototxt)

Usage:
    python3 download-models.py              # Downloads all models
    python3 download-models.py --onnx       # Downloads only ONNX model
    python3 download-models.py --caffe      # Downloads only Caffe model
"""

import argparse
import hashlib
import os
import sys
import urllib.request
from pathlib import Path


class ModelDownloader:
    """Handles downloading and verifying face detection models."""

    # Model definitions with URLs and checksums
    MODELS = {
        "yunet_onnx": {
            "name": "YuNet Face Detection (ONNX)",
            "url": "https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx",
            "filename": "face_detection_yunet_2023mar.onnx",
            "size_mb": 6.3,
            "md5": "ffc0794efcc64025ef5e27b184e47d2f",
            "format": "onnx",
            "description": "YuNet face detector - Modern, efficient ONNX model. Single file, no config needed.",
        },
        "mobilenet_ssd_caffe": {
            "name": "MobileNet-SSD Face Detection (Caffe)",
            "url_model": "https://raw.githubusercontent.com/chuanqi305/MobileNet-SSD/master/mobilenet_iter_73000.caffemodel",
            "url_config": "https://raw.githubusercontent.com/chuanqi305/MobileNet-SSD/master/deploy.prototxt.txt",
            "filename_model": "mobilenet_iter_73000.caffemodel",
            "filename_config": "mobilenet_ssd_deploy.prototxt",
            "size_mb": 23.0,
            "format": "caffe",
            "description": "MobileNet-SSD - Classic, well-tested face detector. Requires .caffemodel and .prototxt files.",
        },
        "res10_300x300_ssd_iter_140000": {
            "name": "ResNet10 Face Detection (Caffe)",
            "url_model": "https://raw.githubusercontent.com/opencv/opencv_3rdparty/dnn_samples_face_detector_20170830/res10_300x300_ssd_iter_140000.caffemodel",
            "url_config": "https://raw.githubusercontent.com/opencv/opencv_3rdparty/dnn_samples_face_detector_20170830/res10_300x300_ssd_deploy.prototxt",
            "filename_model": "res10_300x300_ssd_iter_140000.caffemodel",
            "filename_config": "res10_300x300_ssd_deploy.prototxt",
            "size_mb": 10.0,
            "format": "caffe",
            "description": "ResNet10-SSD - Accurate, medium-weight face detector. Requires .caffemodel and .prototxt files.",
        },
    }

    def __init__(self, models_dir: str = "models"):
        """Initialize the downloader with target directory."""
        self.models_dir = Path(models_dir)
        self.models_dir.mkdir(parents=True, exist_ok=True)

    def download_file(
        self, url: str, filepath: Path, expected_size_mb: float = None
    ) -> bool:
        """
        Download a file from URL with progress reporting.

        Args:
            url: URL to download from
            filepath: Local file path to save to
            expected_size_mb: Expected file size in MB for info display

        Returns:
            True if download successful, False otherwise
        """
        try:
            print(f"  Downloading: {url}")
            if expected_size_mb:
                print(f"  Expected size: {expected_size_mb:.1f} MB")

            def show_progress(block_num: int, block_size: int, total_size: int):
                downloaded = block_num * block_size
                if total_size > 0:
                    percent = min(100, (downloaded * 100) // total_size)
                    mb_downloaded = downloaded / (1024 * 1024)
                    mb_total = total_size / (1024 * 1024)
                    print(
                        f"    Progress: {percent:3d}% ({mb_downloaded:.1f}/{mb_total:.1f} MB)",
                        end="\r",
                    )

            urllib.request.urlretrieve(url, filepath, show_progress)
            print(f"    Download complete!                           ")
            return True

        except Exception as e:
            print(f"  ✗ Download failed: {e}")
            if filepath.exists():
                filepath.unlink()
            return False

    def verify_file(self, filepath: Path, expected_md5: str = None) -> bool:
        """
        Verify downloaded file integrity.

        Args:
            filepath: Path to file to verify
            expected_md5: Expected MD5 hash

        Returns:
            True if verification passed or no hash provided, False otherwise
        """
        if not expected_md5:
            return True

        if not filepath.exists():
            print(f"  ✗ File not found: {filepath}")
            return False

        try:
            md5_hash = hashlib.md5()
            with open(filepath, "rb") as f:
                for chunk in iter(lambda: f.read(4096), b""):
                    md5_hash.update(chunk)

            file_hash = md5_hash.hexdigest()
            if file_hash == expected_md5:
                print(f"  ✓ Verification passed (MD5: {file_hash})")
                return True
            else:
                print(f"  ✗ MD5 mismatch!")
                print(f"    Expected: {expected_md5}")
                print(f"    Got:      {file_hash}")
                return False

        except Exception as e:
            print(f"  ✗ Verification failed: {e}")
            return False

    def download_model(self, model_key: str) -> bool:
        """
        Download a specific model by key.

        Args:
            model_key: Key of model to download

        Returns:
            True if successful, False otherwise
        """
        if model_key not in self.MODELS:
            print(f"✗ Unknown model: {model_key}")
            return False

        model = self.MODELS[model_key]
        print(f"\n{'=' * 70}")
        print(f"Downloading: {model['name']}")
        print(f"{'=' * 70}")
        print(f"Description: {model['description']}")
        print(f"Format: {model['format'].upper()}")
        print(f"Size: ~{model['size_mb']:.1f} MB")

        if model["format"] == "onnx":
            return self._download_onnx_model(model)
        elif model["format"] == "caffe":
            return self._download_caffe_model(model)

        return False

    def _download_onnx_model(self, model: dict) -> bool:
        """Download ONNX model (single file)."""
        filepath = self.models_dir / model["filename"]

        if filepath.exists():
            print(f"✓ File already exists: {filepath}")
            return True

        if not self.download_file(model["url"], filepath, model.get("size_mb")):
            return False

        if not self.verify_file(filepath, model.get("md5")):
            print("  Warning: Verification failed, but file downloaded")

        print(f"✓ Model ready: {filepath}\n")
        return True

    def _download_caffe_model(self, model: dict) -> bool:
        """Download Caffe model (two files: .caffemodel and .prototxt)."""
        # Download model weights
        model_filepath = self.models_dir / model["filename_model"]
        if not model_filepath.exists():
            print("Downloading model weights (.caffemodel):")
            if not self.download_file(
                model["url_model"], model_filepath, model.get("size_mb")
            ):
                return False
        else:
            print(f"✓ Model file already exists: {model_filepath}")

        # Download network config
        config_filepath = self.models_dir / model["filename_config"]
        if not config_filepath.exists():
            print("Downloading network configuration (.prototxt):")
            if not self.download_file(model["url_config"], config_filepath):
                return False
        else:
            print(f"✓ Config file already exists: {config_filepath}")

        print(f"✓ Model ready:")
        print(f"  - Model:  {model_filepath}")
        print(f"  - Config: {config_filepath}\n")
        return True

    def list_models(self):
        """List all available models."""
        print("\n" + "=" * 70)
        print("Available Face Detection Models")
        print("=" * 70)

        for key, model in self.MODELS.items():
            print(f"\n{key}")
            print(f"  Name: {model['name']}")
            print(f"  Format: {model['format'].upper()}")
            print(f"  Size: ~{model['size_mb']:.1f} MB")
            print(f"  Description: {model['description']}")

            if model["format"] == "onnx":
                print(f"  Files needed: 1")
                print(f"    - {model['filename']}")
            else:
                print(f"  Files needed: 2")
                print(f"    - {model['filename_model']}")
                print(f"    - {model['filename_config']}")

    def download_all(self) -> bool:
        """Download all recommended models."""
        print("\n" + "=" * 70)
        print("Downloading All Face Detection Models")
        print("=" * 70)

        # Download YuNet (recommended ONNX)
        success_onnx = self.download_model("yunet_onnx")

        # Download ResNet10 (recommended Caffe)
        success_caffe = self.download_model("res10_300x300_ssd_iter_140000")

        print("\n" + "=" * 70)
        print("Download Summary")
        print("=" * 70)

        if success_onnx:
            print("✓ ONNX model (YuNet) - Ready to use")
            yunet = self.MODELS["yunet_onnx"]
            config_code = f"""
// Using YuNet ONNX model:
FaceTrackerConfig config;
config.model_path = "models/{yunet["filename"]}";
config.config_path = "";  // Not needed for ONNX
config.confidence_threshold = 0.5f;
config.input_width = 320;
config.input_height = 320;
"""
            print(config_code)
        else:
            print("✗ ONNX model (YuNet) - Failed")

        if success_caffe:
            print("\n✓ Caffe model (ResNet10) - Ready to use")
            caffe = self.MODELS["res10_300x300_ssd_iter_140000"]
            config_code = f"""
// Using ResNet10 Caffe model:
FaceTrackerConfig config;
config.model_path = "models/{caffe["filename_model"]}";
config.config_path = "models/{caffe["filename_config"]}";
config.confidence_threshold = 0.5f;
config.input_width = 300;
config.input_height = 300;
"""
            print(config_code)
        else:
            print("\n✗ Caffe model (ResNet10) - Failed")

        return success_onnx or success_caffe


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Download face detection models for FaceTracker",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Download all models
  python3 download-models.py

  # Download only ONNX model
  python3 download-models.py --onnx

  # Download only Caffe models
  python3 download-models.py --caffe

  # List available models
  python3 download-models.py --list

  # Download specific model
  python3 download-models.py --model yunet_onnx
        """,
    )

    parser.add_argument(
        "--onnx",
        action="store_true",
        help="Download only ONNX models",
    )
    parser.add_argument(
        "--caffe",
        action="store_true",
        help="Download only Caffe models",
    )
    parser.add_argument(
        "--model",
        type=str,
        help="Download specific model by key",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List available models",
    )
    parser.add_argument(
        "--models-dir",
        type=str,
        default="models",
        help="Directory to save models (default: models)",
    )

    args = parser.parse_args()

    downloader = ModelDownloader(args.models_dir)

    if args.list:
        downloader.list_models()
        return 0

    if args.model:
        success = downloader.download_model(args.model)
        return 0 if success else 1

    if args.onnx:
        success = downloader.download_model("yunet_onnx")
        return 0 if success else 1

    if args.caffe:
        success = downloader.download_model("res10_300x300_ssd_iter_140000")
        return 0 if success else 1

    # Default: download all
    success = downloader.download_all()
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
