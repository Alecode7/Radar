try:
    from host_app.app import main
except ImportError:
    from app import main


if __name__ == "__main__":
    main()
